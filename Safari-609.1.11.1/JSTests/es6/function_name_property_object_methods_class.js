function test() {

var o = { foo: class {}, bar: class baz {}};
o.qux = class {};
return o.foo.name === "foo" &&
       o.bar.name === "baz" &&
       o.qux.name === "";
      
}

if (!test())
    throw new Error("Test failed");

