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

./prometheus ../vertex_shader.glsl ../fragment_shader.glsl
```

to compile successfully, you need mesa compiled with drm platform 
support and also gbm enabled.
this is satisfied in modern GNU/Linux (tested on iSoft Client OS 
which based on Archlinux). glm is required for opengl es matrix
operation. for compiling demos, glfw3 is also needed.



drm demos
=========
these demos are immature and may harm your video card, try with caution.

* drm_tty demos how to setup kms buffer to render on.
* egl_drm_tty2 demos opengl rendered on egl surface.
* egl_drm_tty3 demos animated opengl on egl surface.
* egl_drm_shining_circle animated circling with bad frame rate (problemmatic driver maybe)
