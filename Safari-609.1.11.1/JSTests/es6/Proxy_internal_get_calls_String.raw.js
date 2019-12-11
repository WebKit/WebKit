function test() {

// String.raw -> Get -> [[Get]]
var get = [];
var raw = new Proxy({length: 2, 0: '', 1: ''}, { get: function(o, k) { get.push(k); return o[k]; }});
var p = new Proxy({raw: raw}, { get: function(o, k) { get.push(k); return o[k]; }});
String.raw(p);
return get + '' === "raw,length,0,1";
      
}

if (!test())
    throw new Error("Test failed");

