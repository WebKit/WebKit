function test() {

class C extends Function {}
var c = new C("return 'foo';");
return c() === 'foo';
      
}

if (!test())
    throw new Error("Test failed");

