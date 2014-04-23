#ifndef _GL_UTIL_H
#define _GL_UTIL_H 

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

struct GLProcess {
    GLuint program, vertex_shader_id, frag_shader_id;
    GLuint vbo;
};

GLProcess* glprocess_create(const char *vertex_path, const char *frag_path);
void glprocess_release(GLProcess* proc);


#endif
