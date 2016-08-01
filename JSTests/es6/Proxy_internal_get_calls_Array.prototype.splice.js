function test() {

// Array.prototype.splice -> Get -> [[Get]]
var get = [];
var p = new Proxy([0,1,2,3], { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.splice.call(p,1,1);
Array.prototype.splice.call(p,1,0,1);
return get + '' === "length,constructor,1,2,3,length,constructor,2,1";
      
}

if (!test())
    throw new Error("Test failed");

