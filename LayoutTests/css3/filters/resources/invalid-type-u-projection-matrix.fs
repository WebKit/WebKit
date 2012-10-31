// If this shader's related test passes, the custom filter does not execute because u_projectionMatrix's type is invalid.

precision mediump float;

// u_projectionMatrix should be a mat4, not a mat3.
uniform mat3 u_projectionMatrix;

void main()
{
    css_MixColor = vec4(1.0, 0.0, 0.0, 1.0);
}
