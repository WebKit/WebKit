// implicit cast of int to float in ternary expression should fail
void main()
{
    float f = true ? 1.0 : 1;
}
