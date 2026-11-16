function test() {

class C {
  static method() { return this === undefined; }
}
return (0,C.method)();
      
}

if (!test())
    throw new Error("Test failed");

