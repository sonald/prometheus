#ifndef _SCENE_H
#define _SCENE_H 

#include <string>
#include "actionmode.h"

class SceneMode: public ActionMode {
    public:
        SceneMode();

        void setThemeFile(const std::string& glsl);
        bool init(int width, int height);
        void deinit();
        void render();
    private:
        std::string _theme; // right now only a frag shader file
};

#endif
