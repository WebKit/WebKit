function test() {

var o = { foo: function(){}, bar: function baz(){}};
o.qux = function(){};
return o.foo.name === "foo" &&
       o.bar.name === "baz" &&
       o.qux.name === "";
      
}

if (!test())
    throw new Error("Test failed");

