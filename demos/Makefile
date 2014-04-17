CXXFLAGS=`pkg-config Qt5Gui glesv2  gbm egl libdrm --cflags`
LDFLAGS=`pkg-config Qt5Gui glesv2  gbm egl libdrm --libs`

CXXFLAGS2=`pkg-config glfw3 glesv2  gbm egl libdrm --cflags`
LDFLAGS2=`pkg-config glfw3 glesv2  gbm egl libdrm --libs`

objs=$(patsubst %.cc,%,$(wilcard *.cc))

all: $(objs)

egl_drm_shining_circle: egl_drm_shining_circle.cc
		clang++ -Wall -x c++ -std=c++11 $(CXXFLAGS2) -o $@ $^ $(LDFLAGS2)
	

%: %.cc
		clang++ -x c++ -fPIC -std=c++11 $(CXXFLAGS) -o $@ $^ $(LDFLAGS)


