function test() {

var foo = function() {};
var bar = function baz() {};
return foo.name === "foo" && bar.name === "baz";
      
}

if (!test())
    throw new Error("Test failed");

