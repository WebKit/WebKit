function test() {

class C {
  method() { return 2; }
}
return typeof C.prototype.method === "function"
  && new C().method() === 2;
      
}

if (!test())
    throw new Error("Test failed");

