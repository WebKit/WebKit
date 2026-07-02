function test() {

class C extends Array {}
var c = new C();
return c.filter(Boolean) instanceof C;
      
}

if (!test())
    throw new Error("Test failed");

