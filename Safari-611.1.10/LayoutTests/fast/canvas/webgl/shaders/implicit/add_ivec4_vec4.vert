// implicit cast adding ivec4 to vec4 should fail
void main()
{
    vec4 f = vec4(1.0, 2.0, 3.0, 4.0) + ivec4(1, 2, 3, 4);
}
