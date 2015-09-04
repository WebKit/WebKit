function test() {

var closed = false;
var iter = global.__createIterableObject([1, 2, 3], {
  'return': function(){ closed = true; return {}; }
});
var add = Set.prototype.add;
Set.prototype.add = function(){ throw 0 };
try {
  new Set(iter);
} catch(e){}
Set.prototype.add = add;
return closed;
      
}

if (!test())
    throw new Error("Test failed");

