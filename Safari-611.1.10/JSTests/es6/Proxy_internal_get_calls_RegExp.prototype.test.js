function test() {

// RegExp.prototype.test -> RegExpExec -> Get -> [[Get]]
var get = [];
var p = new Proxy({ exec: function() { return null; } }, { get: function(o, k) { get.push(k); return o[k]; }});
RegExp.prototype.test.call(p);
return get + '' === "exec";
      
}

if (!test())
    throw new Error("Test failed");

