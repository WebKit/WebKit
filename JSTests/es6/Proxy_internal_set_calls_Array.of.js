function test() {

// Array.from -> Set -> [[Set]]
var set = [];
var p = new Proxy({}, { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
Array.of.call(function(){ return p; }, 1, 2, 3);
return set + '' === "length";
      
}

if (!test())
    throw new Error("Test failed");

