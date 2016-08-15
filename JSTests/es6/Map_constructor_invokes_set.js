function test() {

var passed = false;
var _set = Map.prototype.set;

Map.prototype.set = function(k, v) {
  passed = true;
};

new Map([ [1, 2] ]);
Map.prototype.set = _set;

return passed;
      
}

if (!test())
    throw new Error("Test failed");

