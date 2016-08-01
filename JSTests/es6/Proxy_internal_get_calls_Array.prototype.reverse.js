function test() {

// Array.prototype.reverse -> Get -> [[Get]]
var get = [];
var p = new Proxy([0,,2,,4,,], { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.reverse.call(p);
return get + '' === "length,0,4,2";
      
}

if (!test())
    throw new Error("Test failed");

