precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;
uniform float add;

void main()
{
    // Offset the color value with "add" on each color channel.
    gl_FragColor = texture2D(u_texture, v_texCoord) + add;
}
