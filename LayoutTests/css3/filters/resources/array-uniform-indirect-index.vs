// This shader indirectly indexes a uniform array using other uniforms.

precision mediump float;
precision mediump int;

attribute vec4 a_position;
uniform mat4 u_projectionMatrix;
varying vec4 v_mixColor;

uniform float values[2];
uniform float redIndex;
uniform float greenIndex;
uniform float blueIndex;
uniform float alphaIndex;

void main()
{
    v_mixColor = vec4(values[int(redIndex)], values[int(greenIndex)], values[int(blueIndex)], values[int(alphaIndex)]);
    gl_Position = u_projectionMatrix * a_position;
}
