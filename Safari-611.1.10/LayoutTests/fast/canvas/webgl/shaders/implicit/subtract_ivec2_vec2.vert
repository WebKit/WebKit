// implicit cast of ivec2 to vec2 in subtraction should fail
void main()
{
    vec2 f = vec2(1.0, 2.0) - ivec2(1, 2);
}
