// implicit cast adding integer to vec3 should fail
void main()
{
    vec3 f = vec3(1.0, 2.0, 3.0) + 1;
}
