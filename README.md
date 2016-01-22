Prometheus
=========
A simple plymouth replacement by using egl and drm.

Build
=====
before build, install dependencies. On Ubuntu/Debian
`sudo apt-get install libglfw3-dev libglm-dev`

simple version:

```
mkdir build
cd build
cmake ..
make 
```

to compile successfully, you need mesa compiled with drm platform 
support and also gbm enabled.
this is satisfied in any of modern GNU/Linux distributions. 
glm is required for openGLES matrix
operation. for compiling demos, glfw3 is also needed.

Run
===
right now, there is no installation provided. you need to run it at 
project dir. 

```
./build/prometheusd -m scene -t beamwave_frag.glsl
```

or run text rendering mode
```
sudo ./build/prometheusd -m text
```

if you got multiple video cards, you can use -c (--card) to specify one that 
is in use.
```
sudo ./build/prometheusd -m text --card /dev/dri/card1 -T /dev/tty2
```

**note**: remember to run it at a virtual console.

drm demos
=========
these demos are immature and may harm your video card, try with caution.

* drm_tty demos how to setup kms buffer to render on.
* egl_drm_tty2 demos opengl rendered on egl surface.
* egl_drm_tty3 demos animated opengl on egl surface.
* egl_drm_shining_circle animated circling with bad frame rate (problemmatic driver maybe)


TODO
====
* provide scene mode plugin loader or just use binary blob like altas shaders
