uniform float fval;
uniform vec2 fval2;
uniform vec3 fval3;
uniform vec4 fval4;

void main()
{
    float sum = fval
            + fval2[0] + fval2[1]
            + fval3[0] + fval3[1] + fval3[2]
            + fval4[0] + fval4[1] + fval4[2] + fval4[3];
    gl_Position = vec4(sum, 0.0, 0.0, 1.0);
}
