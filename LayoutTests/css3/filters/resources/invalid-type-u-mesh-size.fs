// If this shader's related test passes, the custom filter does not execute because u_meshSize's type is invalid.

precision mediump float;

// u_meshSize should be a vec2, not a float.
uniform float u_meshSize;

void main()
{
    css_MixColor = vec4(1.0, 0.0, 0.0, 1.0);
}
