function test() {

// Object.assign -> Get -> [[Get]]
var get = [];
var p = new Proxy({foo:1, bar:2}, { get: function(o, k) { get.push(k); return o[k]; }});
Object.assign({}, p);
return get + '' === "foo,bar";
      
}

if (!test())
    throw new Error("Test failed");

