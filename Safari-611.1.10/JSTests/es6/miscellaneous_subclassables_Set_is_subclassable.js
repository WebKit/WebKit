function test() {

var obj = {};
class S extends Set {}
var set = new S();

set.add(123);
set.add(123);

return set.has(123);
      
}

if (!test())
    throw new Error("Test failed");

