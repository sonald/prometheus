#include <cstdio>
#include <cstdlib>
#include <errno.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <algorithm>

#include <QtGui>
using namespace std;

#include <xf86drm.h>
#include <xf86drmMode.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglmesaext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define XRES 800
#define YRES 600
#define BPP  32

const GLchar* vertex_shader = R"(
    attribute vec2 texcoord;
    attribute vec3 color;
    attribute vec2 position;

    varying vec3 Color;
    varying vec2 Texcoord;

    //better calculate MVP outside of shader
    uniform mat4 viewM;
    uniform mat4 modelM;
    uniform mat4 projM;

    void main() {
        Color = color;
        Texcoord = texcoord;
        gl_Position = projM * viewM * modelM * vec4(position, 0.0, 1.0);
    }
)";

const GLchar* frag_shader = R"(
    precision mediump float;
    uniform float time;
    uniform sampler2D kitty;
    uniform sampler2D puppy;
    varying vec3 Color;
    varying vec2 Texcoord;
    void main() {
       vec2 tex = Texcoord;
       float val = (sin(time * 70.0) + 1.0)/2.0;
       gl_FragColor = mix(texture2D(puppy, tex), texture2D(kitty, tex), val);
    }
)";


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

static void init()
{
    program = create_program(vertex_shader, frag_shader);
    if (!program) return;
    glLinkProgram(program);
    glUseProgram(program);


    //GLuint vao;
    //glGenVertexArrays(1, &vao);
    //glBindVertexArray(vao);

    GLuint vbo[3];
    glGenBuffers(3, vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    GLfloat vertex_data[] = {
        -0.5, -0.5,
        -0.5, 0.5,
        0.5, 0.5,
        0.5, -0.5
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_data, vertex_data, GL_STREAM_DRAW);
    ;

    GLint pos_attrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    // colors
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    GLfloat color_data[] = {
        1.0, 1.0, 1.0,
        1.0, 1.0, 1.0,
        1.0, 1.0, 1.0,

        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        0.0, 1.0, 1.0
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof color_data, color_data, GL_STREAM_DRAW);

    GLint color_attrib = glGetAttribLocation(program, "color");
    glEnableVertexAttribArray(color_attrib);
    //offset 9, so use R, G, B colors
    glVertexAttribPointer(color_attrib, 3, GL_FLOAT, GL_FALSE, 0,
            (GLvoid*)(9*sizeof(GLfloat)));

    // 2d texture
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    GLfloat tex_coords[] = {
        0.0, 1.0,
        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof tex_coords, tex_coords, GL_STREAM_DRAW);

    GLint tex_attrib = glGetAttribLocation(program, "texcoord");
    glEnableVertexAttribArray(tex_attrib);
    glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    GLuint tex[2];
    glGenTextures(2, tex);

    QString imgs[] = {"texture.png", "puppy.png"};
    const char *attribNames[] = {"kitty", "puppy"};
    GLenum texutreIds[] = {GL_TEXTURE0, GL_TEXTURE1};

    GLint i = 0;
    std::for_each(std::begin(imgs), std::end(imgs), [&](const QString& src) {
            glActiveTexture(texutreIds[i]);
            glBindTexture(GL_TEXTURE_2D, tex[i]);

            QImage img(src);
            std::cerr << "depths of texture image: " << img.depth()
            << img.bytesPerLine() << img.height()
            << std::endl;
            glTexImage2D(
                GL_TEXTURE_2D, 0,           /* target, level of detail */
                GL_RGBA,                    /* internal format */
                img.width(), img.height(), 0,           /* width, height, border */
                GL_BGRA, GL_UNSIGNED_BYTE,  /* external format, type */
                img.bits()                  /* pixels */
                );

            glUniform1i(glGetUniformLocation(program, attribNames[i]), i);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            i++;
    });


    //elements
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    GLuint elems[] = { //CCW
        0, 1, 2,
        2, 3, 0
    };

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof elems, elems, GL_STREAM_DRAW);

    // setup MVP

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

    glClearColor(0.3, 0.7, 0.7, 0.7);
}

static void render_scene(int width, int height)
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    GLint time = glGetUniformLocation(program, "time");
    float timeval = (float)clock() / CLOCKS_PER_SEC;
    glUniform1f(time, timeval);

    //model
    glm::mat4 modelM;
    modelM = glm::rotate(modelM, timeval * 720.0f, glm::vec3(0.0, 0.0, 1.0));
    glUniformMatrix4fv(glGetUniformLocation(program, "modelM"),
                       1, GL_FALSE, glm::value_ptr(modelM));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glFinish();
}

int main(int argc,char* argv[])
{
    //handle commandline arguments
    int  conn = 0;

    //user variables
    int fd;                                 //drm device handle
    uint32_t oid;                           //old framebuffer id

    //drm system variables
    drmModeRes* resources;                  //resource array
    drmModeConnector* connector;            //connector array
    drmModeEncoder* encoder;                //encoder array
    drmModeModeInfo mode;                   //video mode in use
    drmModeCrtcPtr crtc;                    //crtc pointer

    //open default dri device
    fd = open("/dev/dri/card0",O_RDWR | O_CLOEXEC);
    if(fd<=0) { printf("Couldn't open /dev/dri/card0"); exit(0); }

    struct gbm_device *gbm = gbm_create_device(fd);

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

    EGLDisplay dpy = eglGetDisplay(gbm);
    eglInitialize(dpy, &major, &minor);
    ver = eglQueryString(dpy, EGL_VERSION);
    extensions = eglQueryString(dpy, EGL_EXTENSIONS);
    fprintf(stderr, "ver: %s, ext: %s\n", ver, extensions);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        fprintf(stderr, "%s\n", "need EGL_KHR_surfaceless_context extension");
        exit(1);
    }


    //acquire drm resources
    resources = drmModeGetResources(fd);
    if(resources==0) { printf("drmModeGetResources failed"); exit(0); }
    //*

    //acquire original mode and framebuffer id
    crtc = drmModeGetCrtc(fd,resources->crtcs[0]);
    oid = crtc->buffer_id;
    //*

    int i;

    //acquire drm connector
    for(i=0;i<resources->count_connectors;++i) {
        connector = drmModeGetConnector(fd,resources->connectors[i]);
        if(connector==0 || conn--!=0) { continue; }
        if(connector->connection==DRM_MODE_CONNECTED && connector->count_modes>0) {
            break; 
        }
        drmModeFreeConnector(connector);
    }
    if(i==resources->count_connectors) {
        printf("No active connector found!"); exit(0); 
    }
    //*

    //acquire drm encoder
    for(i=0;i<resources->count_encoders;++i) {
        encoder = drmModeGetEncoder(fd,resources->encoders[i]);
        if(encoder==0) { continue; }
        if(encoder->encoder_id==connector->encoder_id) { break; }
        drmModeFreeEncoder(encoder);
    }
    if(i==resources->count_encoders) {
        printf("No active encoder found!"); exit(0); 
    }

    //check for requested mode
    for(i=0;i<connector->count_modes;++i) {
        mode = connector->modes[i];
        if( (mode.hdisplay==XRES) && (mode.vdisplay==YRES) ) { break; }
    }
    mode = connector->modes[0];
    if(i==connector->count_modes) {
        printf("Requested mode not found!"); exit(0); 
    }
    printf("found mode: %d, %d\n", mode.hdisplay, mode.vdisplay);

    EGLContext ctx;
    eglBindAPI(EGL_OPENGL_ES2_BIT);

    EGLConfig conf;
    int num_conf;
    EGLBoolean ret = eglChooseConfig(dpy, conf_att, &conf, 1, &num_conf);
    if (!ret || num_conf != 1) {
        printf("cannot find a proper EGL framebuffer configuration");
        exit(-1);
    }

    ctx = eglCreateContext(dpy, conf, EGL_NO_CONTEXT, ctx_att);
    if (ctx == EGL_NO_CONTEXT) {
        printf("no context created.\n"); exit(0);
    }
    //eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);

    struct gbm_bo *bo;
    struct gbm_surface *gbmSurface;
    uint32_t handle, stride;

    gbmSurface = gbm_surface_create(gbm, mode.hdisplay,
                      mode.vdisplay, GBM_FORMAT_XRGB8888,
                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gbmSurface) {
        printf("cannot create gbm surface (%d): %m", errno);
        exit(-EFAULT);
    }

    EGLSurface surface = eglCreateWindowSurface(dpy, conf,
                          (EGLNativeWindowType)gbmSurface,
                          NULL);
    if (surface == EGL_NO_SURFACE) {
        printf("cannot create EGL window surface");
        exit(-1);
    }

    if (!eglMakeCurrent(dpy, surface, surface, ctx)) {
        printf("cannot activate EGL context");
        exit(-1);
    }

    if (!gbm_surface_has_free_buffers(gbmSurface)) {
        printf("has no free buffers.");
        exit(-1);
    }

    init();

    uint32_t fb_id = 0; 

    for (int iter = 0; iter < 3000; iter++) {
        if (fb_id) {
            gbm_surface_release_buffer(gbmSurface, bo);
        }
        render_scene(mode.hdisplay, mode.vdisplay);
        printf("render_scene\n");
        if (!eglSwapBuffers(dpy, surface)) {
            printf("cannot swap buffers");
            exit(-1);
        }

        bo = gbm_surface_lock_front_buffer(gbmSurface);
        if (!bo) {
            printf("cannot lock front buffer during creation");
            exit(-1);
        }

        handle = gbm_bo_get_handle(bo).u32;
        stride = gbm_bo_get_stride(bo);
        int width = gbm_bo_get_width(bo);
        int height = gbm_bo_get_height(bo);
        printf("w, h: %d, %d\n", width, height);

        i = drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb_id);
        if(i) { printf("Could not add framebuffer(%d)!", errno); perror("fb"); exit(0); }
        printf("fb_id = %u\n", fb_id);
        i = drmModeSetCrtc(fd, encoder->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, &mode);
        if(i) { printf("Could not set mode!"); exit(0); }

        usleep(33000);
    }


    //wait for enter key
    getchar();
    //*

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(fd);
    //*

    return 0;
}

