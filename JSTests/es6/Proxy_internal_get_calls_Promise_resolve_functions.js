function test() {

// Promise resolve functions -> Get -> [[Get]]
var get = [];
var p = new Proxy({}, { get: function(o, k) { get.push(k); return o[k]; }});
new Promise(function(resolve){ resolve(p); });
return get + '' === "then";
      
}

if (!test())
    throw new Error("Test failed");

