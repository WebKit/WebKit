// implicit cast of int to mat3 in subtraction should fail
void main()
{
    mat3 f = mat3(1.0) - 1;
}
