function test() {

var closed = false;
var iter = global.__createIterableObject([1, 2, 3], {
  'return': function(){ closed = true; return {}; }
});
try {
  Array.from(iter, function() { throw 42 });
} catch(e){}
return closed;
      
}

if (!test())
    throw new Error("Test failed");

