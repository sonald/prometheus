attribute vec2 position;

//better calculate MVP outside of shader
uniform mat4 viewM;
uniform mat4 modelM;
uniform mat4 projM;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
}
