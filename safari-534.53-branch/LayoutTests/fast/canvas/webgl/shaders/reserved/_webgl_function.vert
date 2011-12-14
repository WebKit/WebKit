// use of reserved _webgl prefix as function name should fail
vec4 _webgl_foo() {
  return vec4(1.0);
}

void main() {
  gl_Position = _webgl_foo();
}
