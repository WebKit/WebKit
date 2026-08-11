function test() {

var passed = false;
var _add = WeakSet.prototype.add;

WeakSet.prototype.add = function(v) {
  passed = true;
};

new WeakSet([ { } ]);
WeakSet.prototype.add = _add;

return passed;
      
}

if (!test())
    throw new Error("Test failed");

