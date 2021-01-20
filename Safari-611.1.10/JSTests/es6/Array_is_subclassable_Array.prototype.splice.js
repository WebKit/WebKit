function test() {

class C extends Array {}
var c = new C();
c.push(2,4,6);
return c.splice(1,2) instanceof C;
      
}

if (!test())
    throw new Error("Test failed");

