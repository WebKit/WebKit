function test() {

class B {
  qux() {
    return "quux";
  }
}
class C extends B {
  baz() {
    return x => super.qux();
  }
}
var arrow = new C().baz();
return arrow() === "quux";
      
}

if (!test())
    throw new Error("Test failed");

