function test() {

class C extends Array {}
var c = new C();
c[2] = 'foo';
c.length = 1;
return c.length === 1 && !(2 in c);
      
}

if (!test())
    throw new Error("Test failed");

