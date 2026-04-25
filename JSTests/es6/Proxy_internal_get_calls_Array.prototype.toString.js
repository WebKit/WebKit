function test() {

// Array.prototype.toString -> Get -> [[Get]]
var get = [];
var p = new Proxy({ join:Function() }, { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.toString.call(p);
return get + '' === "join";
      
}

if (!test())
    throw new Error("Test failed");

