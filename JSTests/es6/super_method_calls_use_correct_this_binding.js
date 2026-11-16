function test() {

class B {
  qux(a) { return this.foo + a; }
}
class C extends B {
  qux(a) { return super.qux("bar" + a); }
}
var obj = new C();
obj.foo = "foo";
return obj.qux("baz") === "foobarbaz";
      
}

if (!test())
    throw new Error("Test failed");

