function test() {

var set = new Set();
return set.add(0) === set;
      
}

if (!test())
    throw new Error("Test failed");

