function test() {

// Array.prototype.pop -> Set -> [[Set]]
var set = [];
var p = new Proxy([], { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
p.pop();
return set + '' === "length";
      
}

if (!test())
    throw new Error("Test failed");

