function test() {

// Array.prototype.copyWithin -> Set -> [[Set]]
var set = [];
var p = new Proxy([1,2,3,4,5,6], { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
p.copyWithin(0, 3);
return set + '' === "0,1,2";
      
}

if (!test())
    throw new Error("Test failed");

