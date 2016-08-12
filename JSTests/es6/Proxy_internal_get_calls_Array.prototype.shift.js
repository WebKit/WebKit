function test() {

// Array.prototype.shift -> Get -> [[Get]]
var get = [];
var p = new Proxy([0,1,2,3], { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.shift.call(p);
return get + '' === "length,0,1,2,3";
      
}

if (!test())
    throw new Error("Test failed");

