// use of reserved webgl_ prefix as function name should fail
vec4 webgl_foo() {
  return vec4(1.0);
}

void main() {
  gl_Position = webgl_foo();
}
