#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <cstdarg>
#include <unistd.h>
#include <sys/ioctl.h>

#include <signal.h>
#include <termios.h>
#include <linux/vt.h>

#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

#include "scene.h"
#include "atlas.h"
#include "driver.h"
#include "options.h"

static DisplayContext dc;
static OptionManager* optManager;

static void err_quit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    exit(-1);
    va_end(ap);
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

    dc.action_mode->render();
    if (!eglSwapBuffers(dc.display, dc.surface)) {
        printf("cannot swap buffers");
        exit(-1);
    }

    struct gbm_bo *bo = dc.next_bo = gbm_surface_lock_front_buffer(dc.gbm_surface);
    //printf("next_bo = %lu\n", (unsigned long)bo);
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
    //printf("dc.next_fb_id = %u\n", dc.next_fb_id);

    if (dc.vt_activated && !dc.paused) {
        auto ret = drmModePageFlip(dc.fd, dc.crtc, dc.next_fb_id, 
                DRM_MODE_PAGE_FLIP_EVENT, NULL);
        if (ret) {
            fprintf(stderr, "cannot flip CRTC for connector %u (%d): %m\n",
                    dc.conn, errno);
        } else {
            dc.pflip_pending = true;
        }
    }

    //gettimeofday(&tm_end, NULL);
    //float timeval = (tm_end.tv_sec - tm_start.tv_sec) +
        //(tm_end.tv_usec - tm_start.tv_usec) / 1000000.0;
    //std::cerr << "frame render duration: " << timeval << std::endl;
}

static void modeset_page_flip_event(int fd, unsigned int frame,
        unsigned int sec, unsigned int usec,
        void *data)
{
    //std::cerr << __func__ << " frame: " << frame << std::endl;
    if (dc.vt_activated && !dc.paused)
        dc.pflip_pending = false;
    else
        std::cerr << "vt is deactivated, wait" << std::endl;
}

static void modeset_vblank_handler(int fd, unsigned int sequence, 
        unsigned int tv_sec, unsigned int tv_usec, void *user_data)
{
    std::cerr << __func__ << " sequence: " << sequence << std::endl;
}

// return true if event ask for break of event loop due to error
static bool handleClientEvent()
{
    struct sockaddr_un un;
    socklen_t len = sizeof un;
    int cfd = accept(dc.commid, (struct sockaddr*)&un, &len);
    if (cfd < 0) {
        perror("accept");
        return false;
    }

    char buf[20];
    int n = read(cfd, buf, sizeof buf);
    if (n <= 0) {
        std::cerr << strerror(errno) << std::endl;
        close(cfd);
        return false;
    }

    close(cfd);
    buf[n] = '\0';
    std::cerr << "received command(" << n << "): " << buf << std::endl;
    if (string(buf) == "Q") {
        dc.cleanup = true;
        return true;

    } else if (string(buf) == "P") {
        if (!dc.paused) {
            dc.paused = true;
            drmDropMaster(dc.fd);
        }

    } else if (string(buf) == "R") {
        if (dc.paused) {
            dc.paused = false;
            if (dc.vt_activated && dc.pflip_pending)
                dc.pflip_pending = false;
            drmSetMaster(dc.fd);
            drmModeSetCrtc(dc.fd, dc.crtc, dc.next_fb_id, 0, 0, &dc.conn, 1, &dc.mode);
        }
    }


    return false;
}

static void run_loop()
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
    
    while (1) {
        dc.pflip_pending = true;
        render();

        while (dc.pflip_pending) {
            FD_SET(0, &fds);
            FD_SET(fd, &fds);
            FD_SET(dc.commid, &fds);

            struct timeval tv { 0, 100 };
            ret = select(fd + 1, &fds, NULL, NULL, &tv);
            if (ret < 0) {
                std::cerr << "select: " << strerror(errno) << std::endl;
                break;

            } else if (FD_ISSET(0, &fds)) {
                std::cerr << "exit due to user-input" << std::endl;
                dc.cleanup = true;
                break;

            } else if (FD_ISSET(dc.commid, &fds)) {
                if (handleClientEvent())
                    break;

            } else if (FD_ISSET(fd, &fds)) {
                drmHandleEvent(fd, &ev);
            }
        }

        if (dc.next_bo) {
            gbm_surface_release_buffer(dc.gbm_surface, dc.bo);
            dc.bo = dc.next_bo;
        }

        if (dc.cleanup) break;
    }
}

static void setup_drm()
{
    drmModeRes* resources;                  //resource array
    drmModeConnector* connector;            //connector array
    drmModeEncoder* encoder;                //encoder array

    //open default dri device
    string card = "/dev/dri/card0";
    if (!optManager->value<string>("card").empty()) {
        card = optManager->value<string>("card");
    }
    std::cerr << "open " << card << endl;
    dc.fd = open(card.c_str(), O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (dc.fd <= 0) { 
        err_quit(strerror(errno));
    }

    drmSetMaster(dc.fd);
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

static void on_leave_vt(int sig)
{
    std::cerr << __func__ << std::endl;
    dc.vt_activated = false;
    drmDropMaster(dc.fd);
    ioctl (dc.vtfd, VT_RELDISP, 1);
}

static void on_enter_vt(int sig)
{
    std::cerr << __func__ << std::endl;
    ioctl (dc.vtfd, VT_RELDISP, VT_ACKACQ);
    dc.vt_activated = true;
    drmSetMaster(dc.fd);
    drmModeSetCrtc(dc.fd, dc.crtc, dc.next_fb_id, 0, 0, &dc.conn, 1, &dc.mode);
}

static void setup_vt()
{
    string ttyname = "/dev/tty0";
    if (!optManager->value<string>("tty").empty())
        ttyname = optManager->value<string>("tty");
    dc.vtfd = open(ttyname.c_str(), O_RDWR|O_NOCTTY);
    struct vt_mode mode = {0};

    mode.mode = VT_PROCESS;
    mode.relsig = SIGUSR1;
    mode.acqsig = SIGUSR2;
    if (ioctl(dc.vtfd, VT_SETMODE, &mode) < 0) {
        close(dc.vtfd);
        dc.vtfd = -1;
        perror("ioctl");
        return;
    }

    signal(SIGUSR1, on_leave_vt);
    signal(SIGUSR2, on_enter_vt);
    dc.vt_activated = true;
}

static const char *comm_path = ":prometheus.sock";
static void setup_comm()
{
    struct sockaddr_un un;
    memset(&un, 0, sizeof un);
    un.sun_family = AF_UNIX;
    strncpy(&un.sun_path[1], comm_path, strlen(comm_path));

    int id = socket(AF_UNIX, SOCK_STREAM, 0);
    if (id < 0) {
        perror("socket");
        return;
    }

    if (bind(id, (struct sockaddr*)&un, sizeof un) < 0) {
        perror("bind");
        close(id);
        return;
    }

    if (listen(id, 1) < 0) {
        perror("listen");
        close(id);
        return;
    }

    dc.commid = id;
    std::cerr << "commid = " << dc.commid << std::endl;
}

static void cleanup()
{
    std::cerr << __func__ << std::endl;
    if (dc.commid) {
        close(dc.commid);
    }

    if (dc.vtfd > 0) 
        close(dc.vtfd);

    dc.action_mode->deinit();

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
    drmDropMaster(dc.fd);
    close(dc.fd);
}

int main(int argc, char* argv[])
{
    optManager = OptionManager::get(argc, argv);

    if (!optManager->value<bool>("nodaemon")) {
        daemon(1, 1);
        std::cerr << "daemonize " << optManager->progName() << endl;
    }

    std::setlocale(LC_ALL, "en_US.UTF-8");

    setup_comm();
    setup_drm();
    setup_egl();
    setup_vt();

    if (optManager->value<string>("mode") == "text") {
        std::cerr << "run in text rendering mode" << endl;
        dc.action_mode = new TextMode;
    } else {
        string theme = optManager->value<string>("theme");
        auto m = new SceneMode;
        if (!theme.empty())
            m->setThemeFile(theme);
        dc.action_mode = m;
        
    }
    if (!dc.action_mode->init(dc.mode.hdisplay, dc.mode.vdisplay)) {
        return -1;
    }

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

    run_loop();

    cleanup();

    return 0;
}

