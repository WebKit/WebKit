// Test whether u_meshBox is correctly set; change fragment color to green if yes.
precision mediump float;
uniform vec4 u_meshBox;

bool areFloatsEqual(float a, float b)
{
    const float epsilon = 0.001;
    return (a > (b - epsilon)) && (a < (b + epsilon));
}

bool areVectorsEqual(vec4 a, vec4 b)
{
    return areFloatsEqual(a.x, b.x) && areFloatsEqual(a.y, b.y) && areFloatsEqual(a.z, b.z) && areFloatsEqual(a.w, b.w);
}

void main()
{
    gl_FragColor = areVectorsEqual(u_meshBox, vec4(-0.5, -0.5, 1.0, 1.0)) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
