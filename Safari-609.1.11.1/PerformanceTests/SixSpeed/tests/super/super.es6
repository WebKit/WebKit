
class C {
  constructor() {
    this.foo = 'bar';
  }
  bar() {
    return 41;
  }
}
class D extends C {
  constructor() {
    super();
    this.baz = 'bat';
  }
  bar() {
    return super.bar() + 1;
  }
}
function fn() {
  var d = new D();
  return d.bar();
}

assertEqual(fn(), 42);
test(fn);
