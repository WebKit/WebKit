// implicit cast of vec2 divided by ivec2 should fail
void main()
{
    vec2 f = vec2(1.0, 2.0) / ivec2(1, 2);
}
