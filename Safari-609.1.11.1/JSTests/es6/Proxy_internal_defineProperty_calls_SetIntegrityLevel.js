function test() {

// SetIntegrityLevel -> DefinePropertyOrThrow -> [[DefineOwnProperty]]
var def = [];
var p = new Proxy({foo:1, bar:2}, { defineProperty: function(o, v, desc) { def.push(v); Object.defineProperty(o, v, desc); return true; }});
Object.freeze(p);
return def + '' === "foo,bar";
      
}

if (!test())
    throw new Error("Test failed");

