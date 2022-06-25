function test() {

return Reflect.construct(function(a, b, c) {
  if (new.target === Object) {
    this.qux = a + b + c;
  }
}, ["foo", "bar", "baz"], Object).qux === "foobarbaz";
      
}

if (!test())
    throw new Error("Test failed");

