// use of reserved _webgl prefix as structure name should fail
struct _webgl_Foo {
  int bar;
};

void main() {
  _webgl_Foo foo = _webgl_Foo(1);
}
