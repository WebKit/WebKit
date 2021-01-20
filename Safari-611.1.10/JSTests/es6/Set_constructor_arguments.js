function test() {

var obj1 = {};
var obj2 = {};
var set = new Set([obj1, obj2]);

return set.has(obj1) && set.has(obj2);
      
}

if (!test())
    throw new Error("Test failed");

