// If this shader's related test passes, the custom filter does not execute because a_texCoord's type is invalid.

precision mediump float;

// a_meshCoord should be a vec2, not an vec4.
attribute vec4 a_texCoord;
attribute vec4 a_position;
uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
