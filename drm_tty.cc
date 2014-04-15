// atCROSSLEVEL 2012 ( http://atcrosslevel.de )
// released under zlib/libpng license
// kms.cc
// kms tutorial test
 
#include <cstdio>
#include <cstdlib>
 
#include <sys/mman.h>
 
#include <fcntl.h>
#include <unistd.h>
 
#include <xf86drm.h>
#include <xf86drmMode.h>
 
drmModeModeInfo m800x600 = { 40000,800,840,968,1056,0,600,601,605,628,0,60/*(40000*1000)/(1056*628)*/,0,0,0 }; //clock,hdisplay,hsync_start,hsync_end,htotal,hskew,vdisplay,vsync_start,vsync_end,vtotal,vsync,vrefresh((1000*clock)/(htotal*vtotal)),flags,type,name
 
#define XRES 800
#define YRES 600
#define BPP  32
 
int main(int argc,char* argv[])
{
        //handle commandline arguments
        bool force = 0;
        int  conn = 0;
 
        if(argc==3) {
                force = atoi(argv[1]);
                conn  = atoi(argv[2]);
        }
        //*
 
        //user variables
        int fd;                                 //drm device handle
        uint32_t id;                            //framebuffer id
        uint32_t oid;                           //old framebuffer id
        int* front;                            //pointer to memory mirror of framebuffer
        //*
 
        //drm system variables
        drmModeRes* resources;                  //resource array
        drmModeConnector* connector;            //connector array
        drmModeEncoder* encoder;                //encoder array
        drmModeModeInfo mode;                   //video mode in use
        drmModeCrtcPtr crtc;                    //crtc pointer
        //*
 
        //open default dri device
        fd = open("/dev/dri/card0",O_RDWR | O_CLOEXEC);
        if(fd<=0) { printf("Couldn't open /dev/dri/card0"); exit(0); }
        //*
 
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
                if(connector->connection==DRM_MODE_CONNECTED 
                        && connector->count_modes>0) {
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

        //*
 
        //check for requested mode
        for(i=0;i<connector->count_modes;++i) {
                mode = connector->modes[i];
                if( (mode.hdisplay==XRES) && (mode.vdisplay==YRES) ) { break; }
        }
        if(force==0 && i==connector->count_modes) {
            printf("Requested mode not found!"); exit(0); 
        }
        //*
 
        //force mode
        if(force==1 && i==connector->count_modes) { mode = m800x600; } //test for saved forcable modes
        //*
 
        //setup framebuffer
        struct drm_mode_create_dumb dc = { YRES,XRES,BPP,0,0,0,0 };
        i = drmIoctl(fd,DRM_IOCTL_MODE_CREATE_DUMB,&dc);
        if(i==1) { printf("Could not create buffer object!"); exit(0); }
 
        struct drm_mode_map_dumb dm = { dc.handle,0,0 };
        i = drmIoctl(fd,DRM_IOCTL_MODE_MAP_DUMB,&dm);
        if(i==1) { printf("Could not map buffer object!"); exit(0); }
 
        front = (int*)mmap(0,dc.size,PROT_READ | PROT_WRITE, MAP_SHARED,fd,dm.offset);
        if(front==MAP_FAILED) { printf("Could not mirror buffer object!"); exit(0); }
        //*
 
        //Enlist framebuffer
        i = drmModeAddFB(fd,XRES,YRES,BPP,BPP,dc.pitch,dc.handle,&id);
        if(i==1) { printf("Could not add framebuffer!"); exit(0); }
        //*
 
        //Set Mode
        i = drmModeSetCrtc(fd,encoder->crtc_id,id,0,0,&connector->connector_id,1,&mode);
        if(i==1) { printf("Could not set mode!"); exit(0); }
        //*
 
        //draw testpattern
        for(int i=0;i<YRES;++i) {
            for(int j=0;j<XRES;++j) { front[i*XRES+j] = i*j; } 
        }

        //wait for enter key
        getchar();
 
        //copy back to front and flush front
        drmModeDirtyFB(fd,id,0,0);
        //*
 
        //*
 
        //undo the drm setup in the correct sequence
        drmModeSetCrtc(fd,encoder->crtc_id,oid,0,0,&connector->connector_id,1,&(crtc->mode));
        drmModeRmFB(fd,id);
        munmap(front,dc.size);
        struct drm_mode_map_dumb dd = { dc.handle };
        drmIoctl(fd,DRM_IOCTL_MODE_DESTROY_DUMB,&dd);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        //*
 
        return 0;
}
