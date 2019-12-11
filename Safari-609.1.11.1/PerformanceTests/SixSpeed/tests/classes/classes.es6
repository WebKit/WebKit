class C {
  constructor() {
    this.foo = 'bar';
  }
  bar() {
  }
}

assertEqual(new C().foo, 'bar');

test(function() {
  return new C();
});
