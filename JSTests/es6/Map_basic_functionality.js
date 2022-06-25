function test() {

var key = {};
var map = new Map();

map.set(key, 123);

return map.has(key) && map.get(key) === 123;
      
}

if (!test())
    throw new Error("Test failed");

