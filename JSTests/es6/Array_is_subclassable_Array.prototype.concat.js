function test() {

class C extends Array {}
var c = new C();
return c.concat(1) instanceof C;
      
}

if (!test())
    throw new Error("Test failed");

