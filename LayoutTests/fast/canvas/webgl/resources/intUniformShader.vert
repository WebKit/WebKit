uniform int ival;
uniform ivec2 ival2;
uniform ivec3 ival3;
uniform ivec4 ival4;

void main()
{
    int sum = ival
            + ival2[0] + ival2[1]
            + ival3[0] + ival3[1] + ival3[2]
            + ival4[0] + ival4[1] + ival4[2] + ival4[3];
    gl_Position = vec4(sum, 0.0, 0.0, 1.0);
}
