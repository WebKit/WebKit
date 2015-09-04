function test() {

var closed = false;
var iter = __createIterableObject([1, 2, 3], {
  'return': function(){ closed = true; return {}; }
});
try {
  for (var it of iter) throw 0;
} catch(e){}
return closed;
      
}

if (!test())
    throw new Error("Test failed");

