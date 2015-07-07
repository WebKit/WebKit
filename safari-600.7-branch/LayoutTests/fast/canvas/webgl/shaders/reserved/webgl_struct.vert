// use of reserved webgl_ prefix as structure name should fail
struct webgl_Foo {
  int bar;
};

void main() {
  webgl_Foo foo = webgl_Foo(1);
}
