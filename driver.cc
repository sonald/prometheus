#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <cstdarg>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <GLFW/glfw3.h>

#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <EGL/egl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


struct DisplayContext {
    int fd;                                 //drm device handle
    EGLDisplay display;
    EGLContext gl_context;

    GLuint vbo;
    GLuint vertexShaderId, fragShaderId, program;

    drmModeModeInfo mode;
    uint32_t conn; // connector id
    uint32_t crtc; // crtc id
    drmModeCrtc *saved_crtc;

    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;
    EGLSurface surface;

    struct gbm_bo *bo;
    struct gbm_bo *next_bo;
    struct gbm_bo *third_bo;
    uint32_t next_fb_id; 

    bool pflip_pending;
    bool cleanup;
};

static DisplayContext dc;

static void err_quit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    exit(-1);
    va_end(ap);
}

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


static GLuint create_program(const char *vertexShader, const char *fragShader)
{
    dc.vertexShaderId = create_shader(GL_VERTEX_SHADER, vertexShader);
    dc.fragShaderId = create_shader(GL_FRAGMENT_SHADER, fragShader);
    if (dc.vertexShaderId == 0 || dc.fragShaderId == 0) {
        err_quit("create_program failed\n");
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, dc.vertexShaderId);
    glAttachShader(program, dc.fragShaderId);
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

static void init_gl(int argc, const char** argv)
{
    float width = dc.mode.hdisplay, height = dc.mode.vdisplay;

    char* vertex_shader = nullptr, *frag_shader = nullptr;

    if (argc == 3) {
        vertex_shader = load_shader(argv[1]);
        frag_shader = load_shader(argv[2]);
    } else {
        vertex_shader = load_shader("vertex_shader.glsl");
        frag_shader = load_shader("fragment_shader.glsl");
    }

    dc.program = create_program(vertex_shader, frag_shader);
    free(vertex_shader);
    free(frag_shader);

    glGenBuffers(1, &dc.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, dc.vbo);

    GLfloat vertex_data[] = {
        -1.0, -1.0,
        -1.0, 1.0,
         1.0, 1.0,

         1.0, 1.0,
         1.0, -1.0,
        -1.0, -1.0,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_data, vertex_data, GL_STATIC_DRAW);

    glLinkProgram(dc.program);
    glUseProgram(dc.program);

    GLint pos_attrib = glGetAttribLocation(dc.program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    //projection
    GLint projMId = glGetUniformLocation(dc.program, "projM");
    glm::mat4 projM = glm::perspective(60.0f, width / height, 1.0f, 10.0f);
    glUniformMatrix4fv(projMId, 1, GL_FALSE, glm::value_ptr(projM));

    //view
    GLint viewMId = glGetUniformLocation(dc.program, "viewM");
    auto viewM = glm::lookAt(
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));
    glUniformMatrix4fv(viewMId, 1, GL_FALSE, glm::value_ptr(viewM));

    auto resolution = glm::vec3(width, height, 1.0);
    glUniform3fv(glGetUniformLocation(dc.program, "resolution"),
                 1, glm::value_ptr(resolution));

    glClearColor(0.0, 0.0, 0.0, 1.0);
}

static struct timeval first_time = {0, 0};

static void render_scene(int width, int height)
{
    //glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT);

    struct timeval tm;
    gettimeofday(&tm, NULL);
    if (!first_time.tv_sec) first_time = tm;

    float timeval = (tm.tv_sec - first_time.tv_sec) +
        (tm.tv_usec - first_time.tv_usec) / 1000000.0;
    GLint time = glGetUniformLocation(dc.program, "time");
    glUniform1f(time, timeval);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
    uint32_t fb_id = (uint32_t)(unsigned long)data;
    drmModeRmFB(dc.fd, fb_id);
    std::cerr << __func__ << " destroy fb " << fb_id << std::endl;
}

static uint32_t bo_to_fb(gbm_bo *bo)
{
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);
    int width = gbm_bo_get_width(bo);
    int height = gbm_bo_get_height(bo);

    uint32_t fb_id = 0;
    int ret = drmModeAddFB(dc.fd, width, height, 24, 32, stride, handle, &fb_id);
    gbm_bo_set_user_data(bo, (void*)(unsigned long)fb_id, drm_fb_destroy_callback);
    printf("add new fb = %u\n", fb_id);
    if(ret) { printf("Could not add framebuffer(%d)!", errno); exit(0); }
    return fb_id;
}

static void render()
{
    struct timeval tm_start, tm_end;
    gettimeofday(&tm_start ,NULL);

    render_scene(dc.mode.hdisplay, dc.mode.vdisplay);
    if (!eglSwapBuffers(dc.display, dc.surface)) {
        printf("cannot swap buffers");
        exit(-1);
    }

    struct gbm_bo *bo;
    if (!dc.next_bo) 
        bo = dc.next_bo = gbm_surface_lock_front_buffer(dc.gbm_surface);
    else 
        bo = dc.third_bo = gbm_surface_lock_front_buffer(dc.gbm_surface);

    //printf("next_bo = %lu\n", (unsigned long)dc.next_bo);
    printf("next_bo = %lu\n", (unsigned long)bo);
    if (!bo) {
        printf("cannot lock front buffer during creation");
        exit(-1);
    }

    uint32_t bo_fb = (uint32_t)(unsigned long)gbm_bo_get_user_data(bo);
    if (bo_fb) {
        dc.next_fb_id = bo_fb;
    } else {
        dc.next_fb_id = bo_to_fb(bo);
    }
    printf("dc.next_fb_id = %u\n", dc.next_fb_id);

    auto ret = drmModePageFlip(dc.fd, dc.crtc, dc.next_fb_id, 
            DRM_MODE_PAGE_FLIP_EVENT, NULL);
    if (ret) {
        fprintf(stderr, "cannot flip CRTC for connector %u (%d): %m\n",
                dc.conn, errno);
    } else {
        dc.pflip_pending = true;
    }

    gettimeofday(&tm_end, NULL);
    float timeval = (tm_end.tv_sec - tm_start.tv_sec) +
        (tm_end.tv_usec - tm_start.tv_usec) / 1000000.0;
    std::cerr << "frame render duration: " << timeval << std::endl;
}

static void modeset_page_flip_event(int fd, unsigned int frame,
        unsigned int sec, unsigned int usec,
        void *data)
{
    std::cerr << __func__ << " frame: " << frame << std::endl;
    dc.pflip_pending = false;
}

static void modeset_vblank_handler(int fd, unsigned int sequence, 
        unsigned int tv_sec, unsigned int tv_usec, void *user_data)
{
    std::cerr << __func__ << " sequence: " << sequence << std::endl;
}

static void draw_loop()
{
    int fd = dc.fd;
    int ret;
    fd_set fds;
    drmEventContext ev;

    FD_ZERO(&fds);
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = modeset_page_flip_event;
    ev.vblank_handler = modeset_vblank_handler;
    
    bool first = true;
    while (1) {
        dc.pflip_pending = true;

        render();
        render();

        while (dc.pflip_pending) {
            FD_SET(0, &fds);
            FD_SET(fd, &fds);

            struct timeval tv { 0, 100 };
            ret = select(fd + 1, &fds, NULL, NULL, &tv);
            if (ret < 0) {
                fprintf(stderr, "select() failed with %d: %m\n", errno);
                break;
            } else if (FD_ISSET(0, &fds)) {
                fprintf(stderr, "exit due to user-input\n");
                return;

            } else if (FD_ISSET(fd, &fds)) {
                std::cerr << "handle event" << std::endl;
                drmHandleEvent(fd, &ev);
            }
        }

        if (dc.third_bo) {
            gbm_surface_release_buffer(dc.gbm_surface, dc.bo);
            dc.bo = dc.next_bo;
            gbm_surface_release_buffer(dc.gbm_surface, dc.bo);
            dc.bo = dc.third_bo;
            dc.next_bo = NULL;
        }

    }
}

static void setup_drm()
{
    drmModeRes* resources;                  //resource array
    drmModeConnector* connector;            //connector array
    drmModeEncoder* encoder;                //encoder array

    //open default dri device
    dc.fd = open("/dev/dri/card0", O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (dc.fd <= 0) { 
        err_quit(strerror(errno));
    }

    //acquire drm resources
    resources = drmModeGetResources(dc.fd);
    if(resources == 0) {
        err_quit("drmModeGetResources failed");
    }

    int i;
    //acquire drm connector
    for (i = 0; i < resources->count_connectors; ++i) {
        connector = drmModeGetConnector(dc.fd,resources->connectors[i]);
        if (!connector) continue;
        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            dc.conn = connector->connector_id;
            std::cerr << "find connected connector id " << dc.conn << std::endl;
            break; 
        }
        drmModeFreeConnector(connector);
    }

    if (i == resources->count_connectors) {
        err_quit("No active connector found!");
    }

    encoder = NULL;
    if (connector->encoder_id) {
        encoder = drmModeGetEncoder(dc.fd, connector->encoder_id);
        if(encoder) {
            dc.crtc = encoder->crtc_id;
            drmModeFreeEncoder(encoder);
        }
    }

    if (!encoder) {
        for(i = 0; i < resources->count_encoders; ++i) {
            encoder = drmModeGetEncoder(dc.fd,resources->encoders[i]);
            if(encoder==0) { continue; }
            for (int j = 0; j < resources->count_crtcs; ++j) {
                if (encoder->possible_crtcs & (1<<j)) {
                    dc.crtc = resources->crtcs[j];
                    break;
                }
            }
            drmModeFreeEncoder(encoder);
            if (dc.crtc) break;
        }

        if (i == resources->count_encoders) {
            err_quit("No active encoder found!");
        }
    }

    dc.saved_crtc = drmModeGetCrtc(dc.fd, dc.crtc);

    dc.mode = connector->modes[0];
    printf("\tMode chosen [%s] : Clock => %d, Vertical refresh => %d, Type => %d\n",
            dc.mode.name, dc.mode.clock, dc.mode.vrefresh, dc.mode.type);

    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
}

static void setup_egl()
{
    dc.gbm = gbm_create_device(dc.fd);
    printf("backend name: %s\n", gbm_device_get_backend_name(dc.gbm));

    dc.gbm_surface = gbm_surface_create(dc.gbm, dc.mode.hdisplay,
                      dc.mode.vdisplay, GBM_FORMAT_XRGB8888,
                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!dc.gbm_surface) {
        printf("cannot create gbm surface (%d): %m", errno);
        exit(-EFAULT);
    }


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
        EGL_DEPTH_SIZE, 1,
        EGL_NONE,
    };
    static const EGLint ctx_att[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    dc.display = eglGetDisplay(dc.gbm);
    eglInitialize(dc.display, &major, &minor);
    ver = eglQueryString(dc.display, EGL_VERSION);
    extensions = eglQueryString(dc.display, EGL_EXTENSIONS);
    fprintf(stderr, "ver: %s, ext: %s\n", ver, extensions);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        fprintf(stderr, "%s\n", "need EGL_KHR_surfaceless_context extension");
        exit(1);
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        std::cerr << "bind api failed" << std::endl;
        exit(-1);
    }

    EGLConfig conf;
    int num_conf;
    EGLBoolean ret = eglChooseConfig(dc.display, conf_att, &conf, 1, &num_conf);
    if (!ret || num_conf != 1) {
        printf("cannot find a proper EGL framebuffer configuration");
        exit(-1);
    }

    dc.gl_context = eglCreateContext(dc.display, conf, EGL_NO_CONTEXT, ctx_att);
    if (dc.gl_context == EGL_NO_CONTEXT) {
        printf("no context created.\n"); exit(0);
    }

    dc.surface = eglCreateWindowSurface(dc.display, conf,
                          (EGLNativeWindowType)dc.gbm_surface,
                          NULL);
    if (dc.surface == EGL_NO_SURFACE) {
        printf("cannot create EGL window surface");
        exit(-1);
    }

    if (!eglMakeCurrent(dc.display, dc.surface, dc.surface, dc.gl_context)) {
        printf("cannot activate EGL context");
        exit(-1);
    }
}

static void cleanup()
{
    glDeleteBuffers(1, &dc.vbo);
    glDeleteShader(dc.vertexShaderId);
    glDeleteShader(dc.fragShaderId);
    glDeleteProgram(dc.program);

    eglDestroySurface(dc.display, dc.surface);
    eglDestroyContext(dc.display, dc.gl_context);
    eglTerminate(dc.display);
        
    gbm_surface_destroy(dc.gbm_surface);
    gbm_device_destroy(dc.gbm);

    if (dc.saved_crtc) {
        drmModeSetCrtc(dc.fd, dc.saved_crtc->crtc_id, dc.saved_crtc->buffer_id, 
                dc.saved_crtc->x, dc.saved_crtc->y, &dc.conn, 1, &dc.saved_crtc->mode);
        drmModeFreeCrtc(dc.saved_crtc);
    }
    close(dc.fd);
}

int main(int argc, const char* argv[])
{
    setup_drm();
    setup_egl();
    init_gl(argc, argv);

    if (!eglSwapBuffers(dc.display, dc.surface)) {
        printf("cannot swap buffers");
        exit(-1);
    }

    dc.bo = gbm_surface_lock_front_buffer(dc.gbm_surface);
    printf("first_bo = %lu\n", (unsigned long)dc.bo);
    if (!dc.bo) {
        printf("cannot lock front buffer during creation");
        exit(-1);
    }

    uint32_t fb_id = bo_to_fb(dc.bo);
    auto ret = drmModeSetCrtc(dc.fd, dc.crtc, fb_id, 0, 0, &dc.conn, 1, &dc.mode);
    if(ret) { printf("Could not set mode!"); exit(0); }

    draw_loop();

    cleanup();

    return 0;
}

