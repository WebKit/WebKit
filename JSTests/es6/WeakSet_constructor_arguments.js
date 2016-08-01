function test() {

var obj1 = {}, obj2 = {};
var weakset = new WeakSet([obj1, obj2]);

return weakset.has(obj1) && weakset.has(obj2);
      
}

if (!test())
    throw new Error("Test failed");

