#ifndef _DRIVER_H
#define _DRIVER_H 

#include <xf86drm.h>
#include <xf86drmMode.h>

//include gbm before gl header
#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <EGL/egl.h>

#include "glutil.h"
#include "atlas.h"


struct DisplayContext {
    int fd;                                 //drm device handle
    EGLDisplay display;
    EGLContext gl_context;

    drmModeModeInfo mode;
    uint32_t conn; // connector id
    uint32_t crtc; // crtc id
    drmModeCrtc *saved_crtc;

    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;
    EGLSurface surface;

    struct gbm_bo *bo;
    struct gbm_bo *next_bo;
    uint32_t next_fb_id; 

    bool pflip_pending;
    bool cleanup;

    //vt
    int vtfd;
    bool vt_activated;

    ActionMode* action_mode;
};

#endif
