function test() {

var baz = false;
class C {
  get foo() { return "foo"; }
  set bar(x) { baz = x; }
}
new C().bar = true;
return new C().foo === "foo" && baz;
      
}

if (!test())
    throw new Error("Test failed");

