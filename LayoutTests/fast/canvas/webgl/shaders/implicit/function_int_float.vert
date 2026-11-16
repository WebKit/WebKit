// implicit cast of int to float in function argument should fail
float foo(float f) {
  return f;
}

void main() {
  float f = foo(1);
}
