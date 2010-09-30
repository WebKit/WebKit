// implicit cast of ivec3 to vec3 in not equal comparison should fail
void main()
{
    bool b = vec3(1.0, 2.0, 3.0) != ivec3(1, 2, 3);
}
