// If this shader's related test passes, the custom filter does not execute because a_position's type is invalid.

precision mediump float;

// a_position should be a vec4, not a vec3.
attribute vec3 a_position;

uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * vec4(a_position, 1.0);
}
