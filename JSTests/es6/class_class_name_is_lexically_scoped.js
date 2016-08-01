function test() {

class C {
  method() { return typeof C === "function"; }
}
var M = C.prototype.method;
C = undefined;
return C === undefined && M();
      
}

if (!test())
    throw new Error("Test failed");

