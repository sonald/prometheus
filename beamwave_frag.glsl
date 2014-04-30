precision mediump float;

uniform float time;
uniform vec3 resolution;

vec3 beam(vec2 position) {
    float brightness = 1.0 - abs(position.y - 0.5) * 2.0;
    brightness = smoothstep(0.7, 1.0, brightness);
    return vec3(0.25, 0.75, 1.0) * brightness;
}

vec4 wave(vec2 position, float frequency, float height, float speed, vec3 color) {
    float sinVal = sin(position.x * frequency - time * speed) * height;
    sinVal = sinVal * 0.5 + 0.5;

    float brightness = 1.0 - abs(sinVal - position.y) * 1.5;

    brightness = smoothstep(0.5, 1.2, brightness);
    float opacity = brightness;

    return vec4(color*brightness, opacity);
}

void main( void ) {
    vec2 position = (gl_FragCoord.xy / resolution.xy);
    vec4 result;

    result += wave(position, 2.5, 0.5,  0.2, vec3(0.65, 0.22, 0.47));
    result += wave(position, 3.0, 0.3,  0.5, vec3(0.82, 0.5, 0.07));
    //result += wave(position, 5.0, 0.4, -0.6, vec3(0.0, 1.0, 0.0));

    gl_FragColor =result;
}
