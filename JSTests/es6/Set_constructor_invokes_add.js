function test() {

var passed = false;
var _add = Set.prototype.add;

Set.prototype.add = function(v) {
  passed = true;
};

new Set([1]);
Set.prototype.add = _add;

return passed;
      
}

if (!test())
    throw new Error("Test failed");

