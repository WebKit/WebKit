function test() {

class C extends Function {}
var c = new C("return 'foo';");
return c instanceof C && c instanceof Function && Object.getPrototypeOf(C) === Function;
      
}

if (!test())
    throw new Error("Test failed");

