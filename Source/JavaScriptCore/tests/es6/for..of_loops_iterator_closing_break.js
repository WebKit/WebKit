function test() {

var closed = false;
var iter = __createIterableObject([1, 2, 3], {
  'return': function(){ closed = true; return {}; }
});
for (var it of iter) break;
return closed;
      
}

if (!test())
    throw new Error("Test failed");

