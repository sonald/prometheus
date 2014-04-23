#ifndef _SCENE_H
#define _SCENE_H 

#include "actionmode.h"

class SceneMode: public ActionMode {
    public:
        bool init(int width, int height);
        void deinit();
        void render();
};

#endif
