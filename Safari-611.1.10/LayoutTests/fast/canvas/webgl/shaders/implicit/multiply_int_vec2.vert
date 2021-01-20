// implicit cast of int to vec2 in multiply should fail
void main()
{
    vec2 f = vec2(1.0, 2.0) * 1;
}
