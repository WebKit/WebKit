precision mediump float;
uniform vec4 c1;
uniform vec4 c2;
uniform vec4 c3;
uniform vec4 c4;
uniform vec4 c5;
uniform vec4 c6;

void main()
{
	if (c1 == c2 && c1 == c3 && c1 == c4 && c1 == c5 && c1 == c6)
    	gl_FragColor = c1;
    else 
    	gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
