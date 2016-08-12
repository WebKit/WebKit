function test() {

// IteratorComplete -> Get -> [[Get]]
// IteratorValue -> Get -> [[Get]]
var get = [];
var iterable = {};
iterable[Symbol.iterator] = function() {
  return {
    next: function() {
      return new Proxy({ value: 2, done: false }, { get: function(o, k) { get.push(k); return o[k]; }});
    }
  };
}
var i = 0;
for(var e of iterable) {
  if (++i >= 2) break;
}
return get + '' === "done,value,done,value";
      
}

if (!test())
    throw new Error("Test failed");

