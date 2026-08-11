function test() {

class B {
  qux(a) { return "foo" + a; }
}
class C extends B {
  qux(a) { return super.qux("bar" + a); }
}
return new C().qux("baz") === "foobarbaz";
      
}

if (!test())
    throw new Error("Test failed");

