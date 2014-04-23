#ifndef _ATLASH_H
#define _ATLASH_H 

#include "glutil.h"
#include "actionmode.h"

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H


struct char_info_t {
    float left, top;
    float width, height;
    float ax, ay; //advanced
    float offset; // horizontal offset inside atlas
};

struct atlas_t {
    float width, height;
    struct char_info_t infos[128];    
    int point_size;
};

struct text_mode_t {
    struct atlas_t atlas;
    GLProcess proc;
};

class TextMode: public ActionMode {
    public:
        bool init(int width, int height);
        void deinit();
        void render();

    private:
        atlas_t _atlas;
        void create_atlas(FT_Face face, int pointSize);
        void render_text(const char *text, float x, float y, float sx, float sy);
};

#endif
