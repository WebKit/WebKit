// If this shader's related test passes, the custom filter does not execute because u_meshBox's type is invalid.

precision mediump float;

// u_meshBox should be a vec4, not a vec3.
uniform vec3 u_meshBox;

void main()
{
    css_MixColor = vec4(1.0, 0.0, 0.0, 1.0);
}
