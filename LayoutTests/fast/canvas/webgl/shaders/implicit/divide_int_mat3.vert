// implicit cast of mat3 divided by int should fail
void main()
{
    mat3 f = mat3(1.0) / 1;
}
