// This shader colors alternating tiles in the mesh black or green to create a checkboard pattern.

precision mediump float;
precision mediump int;

attribute vec4 a_position;
attribute vec3 a_triangleCoord;

uniform mat4 u_projectionMatrix;

varying vec4 v_mixColor;

void main()
{
    float green = mod(a_triangleCoord.x + a_triangleCoord.y, 2.0);
    v_mixColor = vec4(0.0, green, 0.0, 1.0);
    gl_Position = u_projectionMatrix * a_position;
}
