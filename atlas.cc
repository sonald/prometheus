#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glutil.h"
#include "atlas.h"

#include <fstream>
#include <iostream>
#include <cstdlib>

#include <sys/time.h>

using namespace std;

static FT_Library ft;
static FT_Face face;
static GLuint tex;

static void init_ft()
{
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "init freetype failed" << std::endl;
        exit(-1);
    }

    if (FT_New_Face(ft, "/usr/share/fonts/microsoft/msyh.ttf", 0, &face)) {
        std::cerr << "load face failed" << std::endl;
        exit(-1);
    }
}

bool TextMode::load_char_helper(FT_ULong char_code)
{
    FT_GlyphSlot slot = face->glyph;
    if (FT_Load_Char(face, char_code, FT_LOAD_RENDER)) {
        std::cerr << "load " << char_code << " failed\n";
        return false;
    }

    _atlas.infos[char_code] = {
        (float)slot->bitmap_left, (float)slot->bitmap_top,
        (float)slot->bitmap.width, (float)slot->bitmap.rows,
        float(slot->advance.x >> 6), float(slot->advance.y >> 6),
        _atlas.width
    };

    _atlas.height = std::max(_atlas.height, _atlas.infos[char_code].height);
    _atlas.width += _atlas.infos[char_code].width + 10;
    return true;
}

//@arg preloads is a wstring which contains all chars that need to load into atlas
void TextMode::create_atlas(FT_Face face, int pointSize, std::wstring preloads)
{
    FT_Set_Pixel_Sizes(face, 0, pointSize);
    _atlas.point_size = pointSize;
    FT_GlyphSlot slot = face->glyph;
    FT_ULong num = 128;
    
    //ASCII is loaded by default
    for (auto i = 32; i < num; i++) {
        load_char_helper(i);
    }

    for (auto i: preloads) {
        load_char_helper(i);
    }

    //NOTE: transparent and `+10` above are anti rendering artifacts.
    //since two chars in texture are close enough to blur each other's border.
    GLubyte transparent[(int)_atlas.width * (int)_atlas.height];
    memset(transparent, 0, sizeof transparent);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, _atlas.width, _atlas.height,
            0, GL_ALPHA, GL_UNSIGNED_BYTE, transparent);

    for (auto p: _atlas.infos) {
        auto i = p.first;
        if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
            std::cerr << "load " << i << " failed\n";
            continue;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, _atlas.infos[i].offset, 0, _atlas.infos[i].width, 
                _atlas.infos[i].height, GL_ALPHA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    }
}

void TextMode::render_str(std::wstring ws, float x, float y, float sx, float sy)
{
    int len = ws.length();

    struct point_t {
        GLfloat x, y, s, t;
    } points[6 * len];

    int i = 0;
    for (auto c: ws) {
        GLfloat x0 = x + _atlas.infos[c].left * sx;
        GLfloat y0 = y + _atlas.infos[c].top * sy;
        GLfloat w = _atlas.infos[c].width * sx, h = _atlas.infos[c].height * sy;

        float tw = _atlas.infos[c].width / _atlas.width;
        float tx = _atlas.infos[c].offset / _atlas.width;
        float ty = _atlas.infos[c].height / _atlas.height;

        int p = i * 6;
        points[p++] = {x0, y0,         tx, 0,};
        points[p++] = {x0 + w, y0,     tx + tw, 0,};
        points[p++] = {x0, y0 - h,     tx, ty,};

        points[p++] = {x0, y0 - h,     tx, ty,};
        points[p++] = {x0 + w, y0,     tx + tw, 0,};
        points[p++] = {x0 + w, y0 - h, tx + tw, ty,};

        x += _atlas.infos[c].ax * sx;
        y += _atlas.infos[c].ay * sy;
        i++;
    }

    glBindBuffer(GL_ARRAY_BUFFER, _proc.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof points, points, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, len * 6);
}

void TextMode::render_text(const char *text, float x, float y, float sx, float sy)
{
    int len = strlen(text);

    struct point_t {
        GLfloat x, y, s, t;
    } points[6 * len];

    for (int i = 0; i < len; ++i) {
        int c = text[i];
        GLfloat x0 = x + _atlas.infos[c].left * sx;
        GLfloat y0 = y + _atlas.infos[c].top * sy;
        GLfloat w = _atlas.infos[c].width * sx, h = _atlas.infos[c].height * sy;

        float tw = _atlas.infos[c].width / _atlas.width;
        float tx = _atlas.infos[c].offset / _atlas.width;
        float ty = _atlas.infos[c].height / _atlas.height;

        int p = i * 6;
        points[p++] = {x0, y0,         tx, 0,};
        points[p++] = {x0 + w, y0,     tx + tw, 0,};
        points[p++] = {x0, y0 - h,     tx, ty,};

        points[p++] = {x0, y0 - h,     tx, ty,};
        points[p++] = {x0 + w, y0,     tx + tw, 0,};
        points[p++] = {x0 + w, y0 - h, tx + tw, ty,};

        x += _atlas.infos[c].ax * sx;
        y += _atlas.infos[c].ay * sy;
    }

    glBindBuffer(GL_ARRAY_BUFFER, _proc.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof points, points, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, len * 6);

}

static timeval tv_start = {0, 0};

bool TextMode::init(int width, int height)
{
    _screenWidth = width, _screenHeight = height;
    init_ft();
    GLProcess* proc = glprocess_create("atlas_vertex.glsl", "atlas_frag.glsl");
    if (!proc) return false;
    _proc = *proc;
    GLuint program = _proc.program;
    glUseProgram(program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenBuffers(1, &_proc.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _proc.vbo);

    GLint pos_attrib = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(program, "tex"), 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    create_atlas(face, 28, L"普华客户端操作系统");

    glClearColor(1.0, 1.0, 1.0, 1.0);
    return true;
}

void TextMode::deinit()
{
    glprocess_release(&_proc);
    _proc.program = 0;
    //...
}

void TextMode::render()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (!tv_start.tv_sec) tv_start = tv;
    float t = (tv.tv_sec - tv_start.tv_sec) + (tv.tv_usec - tv_start.tv_usec) / 1000000.0;

    GLfloat bgcolor[] = {
        float((glm::cos(t) + 1.0)/2.0), float((glm::sin(t)+1.0)/2.0), 0, 1
    };
    glUniform4fv(glGetUniformLocation(_proc.program, "bgcolor"), 1, bgcolor);

    float ps1 = 24.0;
    float sx = 2.0 / _screenWidth, sy = 2.0 / _screenHeight;
        
    float x = -1.0, y = 1.0 - ps1 * sy; 
    render_text("Welcome to iSoft Client OS", x, y, sx, sy);

    GLfloat bgcolor2[] = {
        0, float((glm::cos(t) + 1.0)/2.0), float((glm::sin(t)+1.0)/2.0), 0.5
    };
    glUniform4fv(glGetUniformLocation(_proc.program, "bgcolor"), 1, bgcolor2);
    x = -1.0 + (_screenWidth/2.0) * sx - 0.3, y = 1.0 - (_screenHeight/2.0) * sy; 
    render_str(L"普华客户端操作系统", x, y, sx, sy);

    y -= ps1 * 2 * sy;
    render_str(L"System Loading...", x, y, sx, sy);
}

