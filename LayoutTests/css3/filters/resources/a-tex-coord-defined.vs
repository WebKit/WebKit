// This shader defines a_texCoord. Its associated custom filter should execute properly.

precision mediump float;

attribute vec2 a_texCoord;
attribute vec4 a_position;
uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
