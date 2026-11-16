function test() {

class C extends Function {}
var c = new C("x", "return this.bar + x;");
return c.apply({bar:1}, [2]) === 3;
      
}

if (!test())
    throw new Error("Test failed");

