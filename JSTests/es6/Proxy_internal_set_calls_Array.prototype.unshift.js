function test() {

// Array.prototype.unshift -> Set -> [[Set]]
var set = [];
var p = new Proxy([0,0,,0], { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
p.unshift(0,1);
return set + '' === "5,3,2,0,1,length";
      
}

if (!test())
    throw new Error("Test failed");

