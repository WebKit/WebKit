function test() {

var foo = "method";
class C {
  static [foo]() { return 3; }
}
return typeof C.method === "function"
  && C.method() === 3;
      
}

if (!test())
    throw new Error("Test failed");

