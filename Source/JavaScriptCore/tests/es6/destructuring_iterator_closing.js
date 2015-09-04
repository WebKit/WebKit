function test() {

var closed = false;
var iter = global.__createIterableObject([1, 2, 3], {
  'return': function(){ closed = true; return {}; }
});
var [a, b] = iter;
return closed;
      
}

if (!test())
    throw new Error("Test failed");

