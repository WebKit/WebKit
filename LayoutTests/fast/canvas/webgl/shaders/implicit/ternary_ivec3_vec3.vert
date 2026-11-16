// implicit cast of ivec3 to vec3 in ternary expression should fail
void main()
{
    vec3 f = true ? vec3(1.0, 2.0, 3.0) : ivec3(1, 2, 3);
}
