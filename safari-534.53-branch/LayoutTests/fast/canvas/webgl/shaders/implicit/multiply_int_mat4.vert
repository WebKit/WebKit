// implicit cast of int to mat4 in multiply should fail
void main()
{
    mat4 f = mat4(1.0) * 1;
}
