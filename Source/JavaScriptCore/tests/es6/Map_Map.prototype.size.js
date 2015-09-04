function test() {

var key = {};
var map = new Map();

map.set(key, 123);

return map.size === 1;
      
}

if (!test())
    throw new Error("Test failed");

