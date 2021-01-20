function test() {

var foo = "method";
class C {
  [foo]() { return 2; }
}
return typeof C.prototype.method === "function"
  && new C().method() === 2;
      
}

if (!test())
    throw new Error("Test failed");

