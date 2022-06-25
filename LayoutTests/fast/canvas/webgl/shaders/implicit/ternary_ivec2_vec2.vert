// implicit cast of ivec2 to vec2 in ternary expression should fail
void main()
{
    vec2 f = true ? vec2(1.0, 2.0) : ivec2(1, 2);
}
