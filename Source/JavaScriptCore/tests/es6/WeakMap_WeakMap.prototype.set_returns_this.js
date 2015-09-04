function test() {

var weakmap = new WeakMap();
var key = {};
return weakmap.set(key, 0) === weakmap;
      
}

if (!test())
    throw new Error("Test failed");

