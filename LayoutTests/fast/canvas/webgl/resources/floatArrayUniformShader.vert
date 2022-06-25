uniform float u_floats[4];

void main()
{
	float sum = u_floats[0] + u_floats[1] + u_floats[2] + u_floats[3];
	gl_Position = vec4(sum, 0.0, 0.0, 1.0);
}