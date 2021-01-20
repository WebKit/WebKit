function test() {

var obj1 = {};
var weakset = new WeakSet();

weakset.add(obj1);
weakset.add(obj1);

return weakset.has(obj1);
      
}

if (!test())
    throw new Error("Test failed");

