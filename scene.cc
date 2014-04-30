#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

#include "scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

SceneMode::SceneMode()
    :_theme{"scene_frag.glsl"} // default theme
{
}

void SceneMode::setThemeFile(const std::string& glsl)
{
    this->_theme = glsl;
    //check exists
}

bool SceneMode::init(int width, int height)
{
    _screenWidth = width, _screenHeight = height;
    GLProcess* proc = glprocess_create("scene_vertex.glsl", _theme.c_str());
    if (!proc) return false;
    _proc = *proc;

    glGenBuffers(1, &_proc.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _proc.vbo);

    GLfloat vertex_data[] = {
        -1.0, -1.0,
        -1.0, 1.0,
         1.0, 1.0,

         1.0, 1.0,
         1.0, -1.0,
        -1.0, -1.0,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_data, vertex_data, GL_STATIC_DRAW);

    glUseProgram(_proc.program);

    GLint pos_attrib = glGetAttribLocation(_proc.program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    //projection
    GLint projMId = glGetUniformLocation(_proc.program, "projM");
    glm::mat4 projM = glm::perspective(60.0f, (float)_screenWidth / _screenHeight, 1.0f, 10.0f);
    glUniformMatrix4fv(projMId, 1, GL_FALSE, glm::value_ptr(projM));

    //view
    GLint viewMId = glGetUniformLocation(_proc.program, "viewM");
    auto viewM = glm::lookAt(
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));
    glUniformMatrix4fv(viewMId, 1, GL_FALSE, glm::value_ptr(viewM));

    auto resolution = glm::vec3(_screenWidth, _screenHeight, 1.0);
    glUniform3fv(glGetUniformLocation(_proc.program, "resolution"),
                 1, glm::value_ptr(resolution));

    glClearColor(0.0, 0.0, 0.0, 1.0);
    return true;
}

void SceneMode::deinit()
{
    glprocess_release(&_proc);
}

static struct timeval first_time = {0, 0};
void SceneMode::render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    struct timeval tm;
    gettimeofday(&tm, NULL);
    if (!first_time.tv_sec) first_time = tm;

    float timeval = (tm.tv_sec - first_time.tv_sec) +
        (tm.tv_usec - first_time.tv_usec) / 1000000.0;
    GLint time = glGetUniformLocation(_proc.program, "time");
    glUniform1f(time, timeval);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

