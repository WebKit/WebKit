function test() {

class B {
  constructor(a) { return ["foo" + a]; }
}
class C extends B {
  constructor(a) { return super("bar" + a); }
}
return new C("baz")[0] === "foobarbaz";
      
}

if (!test())
    throw new Error("Test failed");

