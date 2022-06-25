// implicit cast of ivec2 to vec2 in comparision should fail
void main()
{
    bool b = vec2(1.0, 2.0) == ivec2(1, 2);
}
