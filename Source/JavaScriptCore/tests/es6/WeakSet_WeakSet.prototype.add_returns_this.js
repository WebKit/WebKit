function test() {

var weakset = new WeakSet();
var obj = {};
return weakset.add(obj) === weakset;
      
}

if (!test())
    throw new Error("Test failed");

