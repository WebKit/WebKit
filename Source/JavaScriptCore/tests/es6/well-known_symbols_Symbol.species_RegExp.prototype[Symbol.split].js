function test() {

var passed = false;
var obj = { constructor: {} };
obj[Symbol.split] = RegExp.prototype[Symbol.split];
obj.constructor[Symbol.species] = function() {
  passed = true;
  return /./;
};
"".split(obj);
return passed;
      
}

if (!test())
    throw new Error("Test failed");

