function test() {

// HasBinding -> Get -> [[Get]]
var get = [];
var p = new Proxy({foo:1}, { get: function(o, k) { get.push(k); return o[k]; }});
p[Symbol.unscopables] = p;
with(p) {
  typeof foo;
}
return get[0] === Symbol.unscopables && get.slice(1) + '' === "foo";
      
}

if (!test())
    throw new Error("Test failed");

