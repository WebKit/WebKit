function test() {

// Array.prototype.push -> Set -> [[Set]]
var set = [];
var p = new Proxy([], { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
p.push(0,0,0);
return set + '' === "0,1,2,length";
      
}

if (!test())
    throw new Error("Test failed");

