function test() {

var obj = {};
var set = new Set();

set.add(123);
set.add(123);

return set.has(123);
      
}

if (!test())
    throw new Error("Test failed");

