// implicit cast of ivec2 to vec2 in function argument should fail
vec2 foo(vec2 f) {
  return f;
}

void main() {
  vec2 f = foo(ivec2(1, 2));
}
