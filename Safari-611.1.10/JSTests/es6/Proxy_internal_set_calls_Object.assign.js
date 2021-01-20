function test() {

// Object.assign -> Set -> [[Set]]
var set = [];
var p = new Proxy({}, { set: function(o, k, v) { set.push(k); o[k] = v; return true; }});
Object.assign(p, { foo: 1, bar: 2 });
return set + '' === "foo,bar";
      
}

if (!test())
    throw new Error("Test failed");

