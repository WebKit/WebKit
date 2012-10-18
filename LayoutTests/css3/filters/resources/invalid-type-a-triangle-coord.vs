// If this shader's related test passes, the custom filter does not execute because a_triangleCoord's type is invalid.

precision mediump float;

// a_meshCoord should be a vec3, not a vec2.
attribute vec2 a_triangleCoord;

attribute vec4 a_position;

uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
