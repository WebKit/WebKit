// implicit cast of ivec3 to vec3 in function argument should fail
vec3 foo(vec3 f) {
  return f;
}

void main() {
  vec3 f = foo(ivec3(1, 2, 3));
}
