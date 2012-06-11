precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;

uniform mat4 u_projectionMatrix;
uniform float rotateBy;

varying vec2 v_texCoord;

mat4 rotateZMatrix(float f) {
    return mat4(cos(f), sin(f), 0.0, 0.0,
                -sin(f), cos(f), 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);
}

void main()
{
    gl_Position = u_projectionMatrix * rotateZMatrix(rotateBy * 3.14 / 180.0) * a_position;
    v_texCoord = a_texCoord;
}
