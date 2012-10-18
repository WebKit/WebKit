// If this shader's related test passes, the custom filter does not execute because a_position's type is invalid.

precision mediump float;

// a_position should be a vec4, not a vec4 array.
attribute vec4 a_position[1];

uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position[0];
}
