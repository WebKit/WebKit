uniform int u_ints[4];

void main()
{
	int sum = u_ints[0] + u_ints[1] + u_ints[2] + u_ints[3];
	gl_Position = vec4(sum, 0.0, 0.0, 1.0);
}