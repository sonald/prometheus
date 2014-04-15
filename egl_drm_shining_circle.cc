#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

#include <xf86drm.h>
#include <xf86drmMode.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GLFW/glfw3.h>

#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglmesaext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define XRES 800
#define YRES 600
#define BPP  32


static GLuint create_shader(GLenum type, const char *source)
{
    GLuint shader_id = glCreateShader(type);
    if (!shader_id) {
        return 0;
    }

    glShaderSource(shader_id, 1, &source, NULL);
    glCompileShader(shader_id);

    GLint compile_ret;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_ret);
    if (compile_ret == GL_FALSE) {
        char buf[512];
        glGetShaderInfoLog(shader_id, sizeof buf - 1, NULL, buf);
        fprintf(stderr, "%s\n", buf);
        return 0;
    }

    return shader_id;
}

static GLuint vertexShaderId, fragShaderId, program;

static GLuint create_program(const char *vertexShader, const char *fragShader)
{
    vertexShaderId = create_shader(GL_VERTEX_SHADER, vertexShader);
    fragShaderId = create_shader(GL_FRAGMENT_SHADER, fragShader);
    if (vertexShaderId == 0 || fragShaderId == 0) return 0;

    program = glCreateProgram();
    glAttachShader(program, vertexShaderId);
    glAttachShader(program, fragShaderId);
    return program;
}

static char* load_shader(const char* filename)
{
    string src;
    string line;
    ifstream ifs{filename};
    while (getline(ifs, line)) {
        src += line + "\n";
    }

    // cerr << src << endl;
    return strndup(src.c_str(), src.size());
}

static void init(int width, int height)
{
    char* vertex_shader = nullptr, *frag_shader = nullptr;
    vertex_shader = load_shader("vertex_shader.glsl");
    frag_shader = load_shader("fragment_shader.glsl");

    program = create_program(vertex_shader, frag_shader);
    if (!program) return;
    free(vertex_shader);
    free(frag_shader);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    GLfloat vertex_data[] = {
        -1.0, -1.0,
        -1.0, 1.0,
         1.0, 1.0,

         1.0, 1.0,
         1.0, -1.0,
        -1.0, -1.0,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_data, vertex_data, GL_STATIC_DRAW);

    glLinkProgram(program);
    glUseProgram(program);

    GLint pos_attrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    //projection
    GLint projMId = glGetUniformLocation(program, "projM");
    glm::mat4 projM = glm::perspective(60.0f, 640.0f / 480.0f, 1.0f, 10.0f);
    glUniformMatrix4fv(projMId, 1, GL_FALSE, glm::value_ptr(projM));

    GLint viewMId = glGetUniformLocation(program, "viewM");
    //view
    auto viewM = glm::lookAt(
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));
    glUniformMatrix4fv(viewMId, 1, GL_FALSE, glm::value_ptr(viewM));

    auto resolution = glm::vec3(width, height, 1.0);
    glUniform3fv(glGetUniformLocation(program, "resolution"),
                 1, glm::value_ptr(resolution));

    glClearColor(0.0, 0.0, 0.0, 0.7);
}

static struct timeval first_time = {0, 0};

static void render_scene(int width, int height)
{
    //glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    struct timeval tm;
    gettimeofday(&tm, NULL);
    if (!first_time.tv_sec) first_time = tm;

    float timeval = (tm.tv_sec - first_time.tv_sec) +
        (tm.tv_usec - first_time.tv_usec) / 1000000.0;
    std::cerr << "timeval: " << timeval << std::endl;
    GLint time = glGetUniformLocation(program, "time");
    glUniform1f(time, timeval);

    //model
    glm::mat4 modelM;
    modelM = glm::rotate(modelM, timeval * 720.0f, glm::vec3(0.0, 0.0, 1.0));
    glUniformMatrix4fv(glGetUniformLocation(program, "modelM"),
                       1, GL_FALSE, glm::value_ptr(modelM));

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

struct DisplayContext {
    int fd;                                 //drm device handle
    EGLDisplay display;

    unsigned int front_buf;
    int bufs[2];

    drmModeModeInfo mode;
    uint32_t conn; // connector id
    uint32_t crtc; // crtc id

    struct gbm_surface *gbmSurface;
    struct gbm_bo *bo;

    EGLSurface surface;

    bool pflip_pending;
    bool cleanup;
};

static DisplayContext dc;

static void render()
{
    uint32_t handle, stride;
    int ret;

    uint32_t fb_id = 0; 

    if (!gbm_surface_has_free_buffers(dc.gbmSurface) || dc.bo) {
        printf("has no free buffers, free bo.");
        gbm_surface_release_buffer(dc.gbmSurface, dc.bo);
    }

    render_scene(dc.mode.hdisplay, dc.mode.vdisplay);
    printf("render_scene\n");
    if (!eglSwapBuffers(dc.display, dc.surface)) {
        printf("cannot swap buffers");
        exit(-1);
    }

    struct gbm_bo* bo = dc.bo = gbm_surface_lock_front_buffer(dc.gbmSurface);
    if (!dc.bo) {
        printf("cannot lock front buffer during creation");
        exit(-1);
    }

    handle = gbm_bo_get_handle(bo).u32;
    stride = gbm_bo_get_stride(bo);
    int width = gbm_bo_get_width(bo);
    int height = gbm_bo_get_height(bo);
    printf("w, h: %d, %d\n", width, height);

    ret = drmModeAddFB(dc.fd, width, height, 24, 32, stride, handle, &fb_id);
    if(ret) { printf("Could not add framebuffer(%d)!", errno); perror("fb"); exit(0); }
    printf("fb_id = %u\n", fb_id);
    ret = drmModeSetCrtc(dc.fd, dc.crtc, fb_id, 0, 0, &dc.conn, 1, &dc.mode);
    if(ret) { printf("Could not set mode!"); exit(0); }

    ret = drmModePageFlip(dc.fd, dc.crtc, fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
    if (ret) {
        fprintf(stderr, "cannot flip CRTC for connector %u (%d): %m\n", dc.conn, errno);
    } else {
        dc.pflip_pending = true;
    }

}

static void modeset_page_flip_event(int fd, unsigned int frame,
        unsigned int sec, unsigned int usec,
        void *data)
{
    std::cerr << __PRETTY_FUNCTION__ << std::endl;
    dc.pflip_pending = false;
    render();
}

static void draw_loop()
{
    int fd = dc.fd;
    int ret;
    fd_set fds;
    time_t start, cur;
    struct timeval v;
    drmEventContext ev;

    /* init variables */
    srand(time(&start));
    FD_ZERO(&fds);
    memset(&v, 0, sizeof(v));
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = modeset_page_flip_event;

    render();

    int duration = 20;
    /* wait 5s for VBLANK or input events */
    while (time(&cur) < start + duration) {
        FD_SET(0, &fds);
        FD_SET(fd, &fds);
        v.tv_sec = start + duration - cur;

        ret = select(fd + 1, &fds, NULL, NULL, &v);
        if (ret < 0) {
            fprintf(stderr, "select() failed with %d: %m\n", errno);
            break;
        } else if (FD_ISSET(0, &fds)) {
            fprintf(stderr, "exit due to user-input\n");
            break;
        } else if (FD_ISSET(fd, &fds)) {
            std::cerr << "handle event" << std::endl;
            drmHandleEvent(fd, &ev);
        }
    }
}

int main(int argc,char* argv[])
{
    //drm system variables
    drmModeRes* resources;                  //resource array
    drmModeConnector* connector;            //connector array
    drmModeEncoder* encoder;                //encoder array

    //open default dri device
    dc.fd = open("/dev/dri/card0",O_RDWR | O_CLOEXEC);
    if(dc.fd<=0) { printf("Couldn't open /dev/dri/card0"); exit(0); }

    struct gbm_device *gbm = gbm_create_device(dc.fd);

    EGLint major;
    EGLint minor;
    const char *ver, *extensions;

    static const EGLint conf_att[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE,
    };
    static const EGLint ctx_att[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    dc.display = eglGetDisplay(gbm);
    eglInitialize(dc.display, &major, &minor);
    ver = eglQueryString(dc.display, EGL_VERSION);
    extensions = eglQueryString(dc.display, EGL_EXTENSIONS);
    fprintf(stderr, "ver: %s, ext: %s\n", ver, extensions);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        fprintf(stderr, "%s\n", "need EGL_KHR_surfaceless_context extension");
        exit(1);
    }


    //acquire drm resources
    resources = drmModeGetResources(dc.fd);
    if(resources==0) { printf("drmModeGetResources failed"); exit(0); }


    int i;
    //acquire drm connector
    for(i=0;i<resources->count_connectors;++i) {
        connector = drmModeGetConnector(dc.fd,resources->connectors[i]);
        if(connector==0) { continue; }
        if(connector->connection==DRM_MODE_CONNECTED && connector->count_modes>0) {
            dc.conn = connector->connector_id;
            break; 
        }
        drmModeFreeConnector(connector);
    }
    if (i==resources->count_connectors) {
        printf("No active connector found!"); exit(0); 
    }

    //acquire drm encoder
    for(i=0;i<resources->count_encoders;++i) {
        encoder = drmModeGetEncoder(dc.fd,resources->encoders[i]);
        if(encoder==0) { continue; }
        if(encoder->encoder_id==connector->encoder_id) { break; }
        drmModeFreeEncoder(encoder);
    }
    if(i==resources->count_encoders) {
        printf("No active encoder found!"); exit(0); 
    }
    dc.crtc = encoder->crtc_id;

    //check for requested mode
    //for(i=0;i<connector->count_modes;++i) {
        //dc.mode = connector->modes[i];
        //if( (dc.mode.hdisplay==XRES) && (dc.mode.vdisplay==YRES) ) { break; }
    //}
    dc.mode = connector->modes[0];
    if(i==connector->count_modes) {
        printf("Requested mode not found!"); exit(0); 
    }
    printf("found mode: %d, %d\n", dc.mode.hdisplay, dc.mode.vdisplay);

    EGLContext ctx;
    eglBindAPI(EGL_OPENGL_ES2_BIT);

    EGLConfig conf;
    int num_conf;
    EGLBoolean ret = eglChooseConfig(dc.display, conf_att, &conf, 1, &num_conf);
    if (!ret || num_conf != 1) {
        printf("cannot find a proper EGL framebuffer configuration");
        exit(-1);
    }

    ctx = eglCreateContext(dc.display, conf, EGL_NO_CONTEXT, ctx_att);
    if (ctx == EGL_NO_CONTEXT) {
        printf("no context created.\n"); exit(0);
    }
    //eglMakeCurrent(dc.display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);

    dc.gbmSurface = gbm_surface_create(gbm, dc.mode.hdisplay,
                      dc.mode.vdisplay, GBM_FORMAT_XRGB8888,
                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!dc.gbmSurface) {
        printf("cannot create gbm surface (%d): %m", errno);
        exit(-EFAULT);
    }

    dc.surface = eglCreateWindowSurface(dc.display, conf,
                          (EGLNativeWindowType)dc.gbmSurface,
                          NULL);
    if (dc.surface == EGL_NO_SURFACE) {
        printf("cannot create EGL window surface");
        exit(-1);
    }

    if (!eglMakeCurrent(dc.display, dc.surface, dc.surface, ctx)) {
        printf("cannot activate EGL context");
        exit(-1);
    }

    if (!gbm_surface_has_free_buffers(dc.gbmSurface)) {
        printf("has no free buffers.");
        exit(-1);
    }

    init(dc.mode.hdisplay, dc.mode.vdisplay);
    draw_loop();

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(dc.fd);
    //*

    return 0;
}

