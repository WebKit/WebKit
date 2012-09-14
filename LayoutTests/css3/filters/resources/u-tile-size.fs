// Test whether u_tileSize is correctly set; change fragment color to green if yes.
precision mediump float;
uniform vec2 u_tileSize;

bool areFloatsEqual(float a, float b)
{
    const float epsilon = 0.001;
    return (a > (b - epsilon)) && (a < (b + epsilon));
}

bool areVectorsEqual(vec2 a, vec2 b)
{
    return areFloatsEqual(a.x, b.x) && areFloatsEqual(a.y, b.y);
}

void main()
{
    gl_FragColor = areVectorsEqual(u_tileSize, vec2(0.05, 0.10)) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
