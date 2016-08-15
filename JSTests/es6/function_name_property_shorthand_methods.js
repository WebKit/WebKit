function test() {

var o = { foo(){} };
return o.foo.name === "foo";
      
}

if (!test())
    throw new Error("Test failed");

