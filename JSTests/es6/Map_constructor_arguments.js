function test() {

var key1 = {};
var key2 = {};
var map = new Map([[key1, 123], [key2, 456]]);

return map.has(key1) && map.get(key1) === 123 &&
       map.has(key2) && map.get(key2) === 456;
      
}

if (!test())
    throw new Error("Test failed");

