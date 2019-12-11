function test() {

// Array.from -> Get -> [[Get]]
var get = [];
var p = new Proxy({length: 2, 0: '', 1: ''}, { get: function(o, k) { get.push(k); return o[k]; }});
Array.from(p);
return get[0] === Symbol.iterator && get.slice(1) + '' === "length,0,1";
      
}

if (!test())
    throw new Error("Test failed");

