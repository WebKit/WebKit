uniform mat2 mval2;
uniform mat3 mval3;
uniform mat4 mval4;

void main()
{
    gl_Position = vec4(mval2 * vec2(1.0, 2.0), 0.0, 0.0)
            + vec4(mval3 * vec3(1.0, 2.0, 3.0), 0.0)
            + mval4 * vec4(1.0, 2.0, 3.0, 4.0);
}
