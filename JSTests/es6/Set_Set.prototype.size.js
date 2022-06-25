function test() {

var obj = {};
var set = new Set();

set.add(123);
set.add(123);
set.add(456);

return set.size === 2;
      
}

if (!test())
    throw new Error("Test failed");

