// RegExp.prototype.toString -> Get -> [[Get]]
function test() {
    var get = [];
    var p = new Proxy({}, { get: function(o, k) { get.push(k); return o[k]; }});
    RegExp.prototype.toString.call(p);
    return get + '' === "source,flags";
}

if (!test())
    throw new Error("Test failed.")
