uniform bool bval;
uniform bvec2 bval2;
uniform bvec3 bval3;
uniform bvec4 bval4;

void main()
{
    bool allSet = bval
            && bval2[0] && bval2[1]
            && bval3[0] && bval3[1] && bval3[2]
            && bval4[0] && bval4[1] && bval4[2] && bval4[3];
    gl_Position = vec4((allSet ? 1.0 : -1.0), 0.0, 0.0, 1.0);
}
