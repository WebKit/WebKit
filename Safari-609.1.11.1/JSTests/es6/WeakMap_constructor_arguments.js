function test() {

var key1 = {};
var key2 = {};
var weakmap = new WeakMap([[key1, 123], [key2, 456]]);

return weakmap.has(key1) && weakmap.get(key1) === 123 &&
       weakmap.has(key2) && weakmap.get(key2) === 456;
      
}

if (!test())
    throw new Error("Test failed");

