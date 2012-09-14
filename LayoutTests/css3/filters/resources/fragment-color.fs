precision mediump float;
uniform float colorArray[4];

void main()
{
    gl_FragColor = vec4(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
}
