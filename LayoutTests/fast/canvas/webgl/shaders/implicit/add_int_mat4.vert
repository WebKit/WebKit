// implicit cast adding integer to mat4 should fail
void main()
{
    mat4 f = mat4(1.0) + 1;
}
