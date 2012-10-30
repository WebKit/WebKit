// If this shader's related test passes, the custom filter does not execute because my_attribute is not one of the built-in attributes.

precision mediump float;

attribute float my_attribute;
attribute vec4 a_position;
uniform mat4 u_projectionMatrix;

void main()
{
    gl_Position = u_projectionMatrix * a_position;
}
