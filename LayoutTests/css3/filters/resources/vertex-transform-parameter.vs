precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;

uniform mat4 u_projectionMatrix;
uniform mat4 transform;
varying vec2 v_texCoord;

void main()
{
    gl_Position = u_projectionMatrix * transform * a_position;
    v_texCoord = a_texCoord;
}
