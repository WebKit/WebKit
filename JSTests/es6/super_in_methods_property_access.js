function test() {

class B {}
B.prototype.qux = "foo";
B.prototype.corge = "baz";
class C extends B {
  quux(a) { return super.qux + a + super["corge"]; }
}
C.prototype.qux = "garply";
return new C().quux("bar") === "foobarbaz";
      
}

if (!test())
    throw new Error("Test failed");

