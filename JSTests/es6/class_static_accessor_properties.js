function test() {

var baz = false;
class C {
  static get foo() { return "foo"; }
  static set bar(x) { baz = x; }
}
C.bar = true;
return C.foo === "foo" && baz;
      
}

if (!test())
    throw new Error("Test failed");

