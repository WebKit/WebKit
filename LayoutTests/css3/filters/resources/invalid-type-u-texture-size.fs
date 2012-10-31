// If this shader's related test passes, the custom filter does not execute because u_textureSize's type is invalid.

precision mediump float;

// u_textureSize should be a vec2, not a vec3.
uniform vec3 u_textureSize;

void main()
{
    css_MixColor = vec4(1.0, 0.0, 0.0, 1.0);
}
