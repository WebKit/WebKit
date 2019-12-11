function test() {

class C extends Promise {}
var c = new C(function(resolve, reject) { resolve("foo"); });
return c instanceof C && c instanceof Promise && Object.getPrototypeOf(C) === Promise;
      
}

if (!test())
    throw new Error("Test failed");

