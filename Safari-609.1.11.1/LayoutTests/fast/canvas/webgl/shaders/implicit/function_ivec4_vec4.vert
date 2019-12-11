// implicit cast of ivec4 to vec4 in function argument should fail
vec4 foo(vec4 f) {
  return f;
}

void main() {
  vec4 f = foo(ivec4(1, 2, 3, 4));
}
