function test() {

var f = Object.freeze({});
var m = new WeakMap;
m.set(f, 42);
return m.get(f) === 42;
      
}

if (!test())
    throw new Error("Test failed");

