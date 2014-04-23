precision mediump float;
uniform vec4 bgcolor;
uniform sampler2D tex;

varying vec2 texcoord;

void main()
{
    gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texcoord).a) * bgcolor;
}
