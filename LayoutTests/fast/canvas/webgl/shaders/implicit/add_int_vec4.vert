// implicit cast adding integer to vec4 should fail
void main()
{
    vec4 f = vec4(1.0, 2.0, 3.0, 4.0) + 1;
}
