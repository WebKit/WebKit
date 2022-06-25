// implicit cast of int to mat2 in multiply should fail
void main()
{
    mat2 f = mat2(1.0) * 1;
}
