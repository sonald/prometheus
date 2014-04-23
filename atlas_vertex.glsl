attribute vec4 position;
varying vec2 texcoord;

void main() {
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texcoord = position.zw; 
}
