function test() {

// Object.assign -> [[GetOwnProperty]]
var gopd = [];
var p = new Proxy({foo:1, bar:2},
  { getOwnPropertyDescriptor: function(o, v) { gopd.push(v); return Object.getOwnPropertyDescriptor(o, v); }});
Object.assign({}, p);
return gopd + '' === "foo,bar";
      
}

if (!test())
    throw new Error("Test failed");

