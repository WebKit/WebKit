precision mediump float;
uniform vec4 color;
varying vec2 v_texCoord;
void main()
{
    // Mix the texture with the color specified as an uniform.
    // Also apply a gradient to check for the right direction.
    // The color should be lightest at the top and darkest at the bottom.
    css_MixColor = vec4(color.xyz, color.a * v_texCoord.y);
}
