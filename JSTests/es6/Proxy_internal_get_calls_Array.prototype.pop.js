function test() {

// Array.prototype.pop -> Get -> [[Get]]
var get = [];
var p = new Proxy([0,1,2,3], { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.pop.call(p);
return get + '' === "length,3";
      
}

if (!test())
    throw new Error("Test failed");

