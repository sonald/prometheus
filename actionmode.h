#ifndef _ACTION_MODE_H
#define _ACTION_MODE_H 

#include "glutil.h"

class  ActionMode {
    public:
        virtual bool init(int width, int height) = 0;
        virtual void deinit() = 0;
        virtual void render() = 0;
        GLProcess& process() { return _proc; }

    protected:
        struct GLProcess _proc;
        int _screenWidth;
        int _screenHeight;
};

#endif
