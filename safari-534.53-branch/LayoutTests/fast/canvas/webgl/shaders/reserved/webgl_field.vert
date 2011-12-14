// use of reserved webgl_ prefix as structure field should fail
struct Foo {
  int webgl_bar;
};

void main() {
  Foo foo = Foo(1);
}
