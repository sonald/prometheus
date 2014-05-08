#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

#include "glutil.h"

static string load_shader(const char* filename)
{
    ifstream ifs{filename};
    if (!ifs) {
        cerr << "can not open file: " << filename << endl;
        return "";
    }

    string src;
    string line;
    while (getline(ifs, line)) {
        src += line + "\n";
    }

    return src;
}

static GLuint create_shader(GLenum type, const char *source)
{
    GLuint shader_id = glCreateShader(type);
    if (!shader_id) {
        return 0;
    }

    glShaderSource(shader_id, 1, &source, NULL);
    glCompileShader(shader_id);

    GLint ret;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &ret);
    if (ret == GL_FALSE) {
        char buf[512];
        glGetShaderInfoLog(shader_id, sizeof buf - 1, NULL, buf);
        cerr << buf << endl;
        return 0;
    }

    return shader_id;
}

GLProcess* glprocess_create(const char *vertex_path, const char *frag_path,
        bool inmemory)
{
    string vertex_shader, frag_shader;
    if (inmemory) {
        vertex_shader = {vertex_path};
        frag_shader = {frag_path};
    } else {
        vertex_shader = load_shader(vertex_path);
        frag_shader = load_shader(frag_path);
    }

    if (frag_shader.empty() || (vertex_shader.empty())) {
        return nullptr;
    }


    GLProcess* proc = new GLProcess;
    proc->vertex_shader_id = create_shader(GL_VERTEX_SHADER, vertex_shader.c_str());
    proc->frag_shader_id= create_shader(GL_FRAGMENT_SHADER, frag_shader.c_str());
    if (proc->vertex_shader_id == 0 || proc->frag_shader_id == 0) {
        delete proc;
        return nullptr;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, proc->vertex_shader_id);
    glAttachShader(program, proc->frag_shader_id);
    glLinkProgram(program);
    //TODO: check link error


    proc->program = program;
    return proc;
}

void glprocess_release(GLProcess* proc)
{
    glDeleteBuffers(1, &proc->vbo);
    glDeleteShader(proc->vertex_shader_id);
    glDeleteShader(proc->frag_shader_id);
    glDeleteProgram(proc->program);
}
