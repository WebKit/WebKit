precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D s_texture;
void main()
{
    // Offset the color value with 0.3 on each color channel.
    gl_FragColor = texture2D(s_texture, v_texCoord) + 0.3;
}