function test() {

var foo = class {};
var bar = class baz {};
var qux = class { static name() {} };
return foo.name === "foo" &&
       bar.name === "baz" &&
       typeof qux.name === "function";
      
}

if (!test())
    throw new Error("Test failed");

