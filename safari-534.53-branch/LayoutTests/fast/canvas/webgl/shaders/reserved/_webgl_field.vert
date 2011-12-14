// use of reserved _webgl prefix as structure field should fail
struct Foo {
  int _webgl_bar;
};

void main() {
  Foo foo = Foo(1);
}
