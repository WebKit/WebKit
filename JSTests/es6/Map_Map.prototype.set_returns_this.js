function test() {

var map = new Map();
return map.set(0, 0) === map;
      
}

if (!test())
    throw new Error("Test failed");

