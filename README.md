Prometheus
=========
A simple plymouth replacement by using egl and drm.

Build
=====
simple guide:

```
mkdir build
cd build
cmake ..
make 

```

to compile successfully, you need mesa compiled with drm platform 
support and also gbm enabled.
this is satisfied in modern GNU/Linux (tested on iSoft Client OS 
which is based on Archlinux). glm is required for opengl es matrix
operation. for compiling demos, glfw3 is also needed.

Run
===
right now, there is no installation provided. you need to run it at 
project dir. 

```
./build/prometheus -m scene -t beamwave_frag.glsl
```

**note**: remember to run it at a virtual console.

drm demos
=========
these demos are immature and may harm your video card, try with caution.

* drm_tty demos how to setup kms buffer to render on.
* egl_drm_tty2 demos opengl rendered on egl surface.
* egl_drm_tty3 demos animated opengl on egl surface.
* egl_drm_shining_circle animated circling with bad frame rate (problemmatic driver maybe)
