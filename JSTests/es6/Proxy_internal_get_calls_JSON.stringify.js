function test() {

// JSON.stringify -> Get -> [[Get]]
var get = [];
var p = new Proxy({}, { get: function(o, k) { get.push(k); return o[k]; }});
JSON.stringify(p);
return get + '' === "toJSON";
      
}

if (!test())
    throw new Error("Test failed");

