// This shader should not execute if the mesh is attached because a_triangleCoord is only available in detached meshses.

precision mediump float;

attribute vec3 a_triangleCoord;
attribute vec4 a_position;
uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
