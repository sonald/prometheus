precision mediump float;

uniform vec4 bgcolor;
uniform sampler2D tex;
uniform float time;
uniform vec3 resolution;

varying vec2 texcoord;

#define PI 3.1415926

vec4 effects() {
    vec2 p = ( gl_FragCoord.xy / resolution.xy ) - (cos(time) + 2.0)/5.;
    float sx = 0.3 * (p.x + 0.8) * sin( 5.0 * p.x - 2. * time);
    float dy = 4./ ( 30. * abs(p.y - sx));
    dy += 1./ (60. * length(p - vec2(p.x, 0.)));
    return vec4( (p.x + 0.4) * dy, 0.3 * dy, dy, 1.0 );
}

void main()
{
    /*gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texcoord).a) * bgcolor;*/
    gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texcoord).a) * effects();
}
