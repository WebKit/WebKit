precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;

uniform float perspective;
uniform vec2 vertex_offset;
uniform vec3 rotate;

uniform mat4 u_projectionMatrix;
varying vec2 v_texCoord;

const float M_PI = 3.14159265;

mat4 perspectiveMatrix(float p) {
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, - 1.0 / p,
                0.0, 0.0, 0.0, 1.0);
}

mat4 translateMatrix(vec2 t) {
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                t.x, t.y, 0.0, 1.0);
}

mat4 rotateXMatrix(float f) {
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, cos(f), sin(f), 0.0,
                0.0, -sin(f), cos(f), 0.0,
                0.0, 0.0, 0.0, 1.0);
}

mat4 rotateYMatrix(float f) {
    return mat4(cos(f), 0.0, -sin(f), 0.0,
                0.0, 1.0, 0.0, 0.0,
                sin(f), 0, cos(f), 0.0,
                0.0, 0.0, 0.0, 1.0);
}

mat4 rotateZMatrix(float f) {
    return mat4(cos(f), sin(f), 0.0, 0.0,
                -sin(f), cos(f), 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);
}

mat4 rotateMatrix(vec3 a) {
    return rotateXMatrix(a.x) * rotateYMatrix(a.y) * rotateZMatrix(a.z);
}

void main()
{
    // Use the values from CSS to change the perspective, translate and rotate the surface. This way we can test
    // float, vec2 and vec3. Note that vec4 is tested in the fragment shader vertex-offset-parameters.fs.
    gl_Position = u_projectionMatrix * perspectiveMatrix(perspective) * translateMatrix(vertex_offset) * rotateMatrix(rotate * M_PI / 180.0) * a_position;
    
    v_texCoord = a_texCoord;
}
