// implicit cast from int to float in struct initializer should fail
struct Foo {
  float bar;
};

void main() {
  Foo foo = Foo(1);
}
