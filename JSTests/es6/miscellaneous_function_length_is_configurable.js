function test() {

var fn = function(a, b) {};

var desc = Object.getOwnPropertyDescriptor(fn, "length");
if (desc.configurable) {
  Object.defineProperty(fn, "length", { value: 1 });
  return fn.length === 1;
}

return false;
      
}

if (!test())
    throw new Error("Test failed");

