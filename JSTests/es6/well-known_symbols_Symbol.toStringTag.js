function test() {

var a = {};
a[Symbol.toStringTag] = "foo";
return (a + "") === "[object foo]";
      
}

if (!test())
    throw new Error("Test failed");

