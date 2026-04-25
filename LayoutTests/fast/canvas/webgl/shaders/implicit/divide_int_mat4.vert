// implicit cast of mat4 divided by int should fail
void main()
{
    mat4 f = mat4(1.0) / 1;
}
