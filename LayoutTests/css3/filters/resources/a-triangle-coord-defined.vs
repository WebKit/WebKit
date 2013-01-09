// This shader defines a_triangleCoord. If the mesh type is detached, its associated custom filter should execute properly.

precision mediump float;

attribute vec3 a_triangleCoord;
attribute vec4 a_position;
uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
