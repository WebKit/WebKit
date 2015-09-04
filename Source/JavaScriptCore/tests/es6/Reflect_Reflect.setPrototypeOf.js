function test() {

var obj = {};
Reflect.setPrototypeOf(obj, Array.prototype);
return obj instanceof Array;
      
}

if (!test())
    throw new Error("Test failed");

