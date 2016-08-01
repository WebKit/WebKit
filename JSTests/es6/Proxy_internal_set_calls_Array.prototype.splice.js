function test() {

// Array.prototype.splice -> Set -> [[Set]]
var set = [];
var p = new Proxy([1,2,3], { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
p.splice(1,0,0);
return set + '' === "3,2,1,length";
      
}

if (!test())
    throw new Error("Test failed");

