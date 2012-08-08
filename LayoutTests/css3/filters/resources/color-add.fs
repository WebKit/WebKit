precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;
uniform float add;

void main()
{
    // Offset the color value with "add" on each color channel.
    vec4 sourceColor = texture2D(u_texture, v_texCoord);
    gl_FragColor = vec4(sourceColor.xyz + add, sourceColor.a);
}
