function test() {

var key = {};
var weakmap = new WeakMap();

weakmap.set(key, 123);

return weakmap.has(key) && weakmap.get(key) === 123;
      
}

if (!test())
    throw new Error("Test failed");

