function test() {

class C extends Array {}
var c = new C();
return c instanceof C && c instanceof Array && Object.getPrototypeOf(C) === Array;
      
}

if (!test())
    throw new Error("Test failed");

