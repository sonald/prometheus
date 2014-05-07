precision mediump float;

uniform vec4 bgcolor;
uniform sampler2D tex;
uniform float time;
uniform vec3 resolution;

varying vec2 texcoord;

#define PI 3.1415926

vec4 effects(){
    vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
    vec2 t = vec2(gl_FragCoord.xy / resolution.xy);
    
    vec3 c = vec3(0);
    
    for(int i = 0; i < 10; i++) {
            float t = 0.4 * PI * float(i) / 30.0 * time * 0.5;
            float x = cos(3.0*t);
            float y = sin(4.0*t);
            vec2 o = 0.2 * vec2(x, y);
            float r = fract(x);
            float g = 0.5 - r;
            c += 0.02 / (length(p-o)) * vec3(r, g, 0.9);
        }
    
    return vec4(c, 1);
}

void main()
{
    /*gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texcoord).a) * bgcolor;*/
    gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texcoord).a) * effects();
}
