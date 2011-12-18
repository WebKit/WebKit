precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec3 a_triangleCoord;

uniform mat4 u_projectionMatrix;
varying vec2 v_texCoord;

void main()
{
    gl_Position = u_projectionMatrix * (a_position + vec4(a_triangleCoord.x / 4.0, a_triangleCoord.y / 4.0, 0.0, 0.0));
    v_texCoord = a_texCoord;
}
