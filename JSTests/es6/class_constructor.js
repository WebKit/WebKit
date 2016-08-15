function test() {

class C {
  constructor() { this.x = 1; }
}
return C.prototype.constructor === C
  && new C().x === 1;
      
}

if (!test())
    throw new Error("Test failed");

