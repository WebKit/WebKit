// implicit cast adding integer to mat3 should fail
void main()
{
    mat3 f = mat3(1.0) + 1;
}
