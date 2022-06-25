// implicit cast of vec3 divided by ivec3 should fail
void main()
{
    vec3 f = vec3(1.0, 2.0, 3.0) / ivec3(1, 2, 3);
}
