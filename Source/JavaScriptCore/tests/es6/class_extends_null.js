function test() {

class C extends null {
  constructor() { return Object.create(null); }
}
return Function.prototype.isPrototypeOf(C)
  && Object.getPrototypeOf(C.prototype) === null;
      
}

if (!test())
    throw new Error("Test failed");

