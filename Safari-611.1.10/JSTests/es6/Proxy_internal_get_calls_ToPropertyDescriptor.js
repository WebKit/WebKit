function test() {

// ToPropertyDescriptor -> Get -> [[Get]]
var get = [];
var p = new Proxy({
    enumerable: true, configurable: true, value: true,
    writable: true, get: Function(), set: Function()
  }, { get: function(o, k) { get.push(k); return o[k]; }});
try {
  // This will throw, since it will have true for both "get" and "value",
  // but not before performing a Get on every property.
  Object.defineProperty({}, "foo", p);
} catch(e) {
  return get + '' === "enumerable,configurable,value,writable,get,set";
}
      
}

if (!test())
    throw new Error("Test failed");

