function test() {

class C { static foo(){} };
return C.foo.name === "foo";
      
}

if (!test())
    throw new Error("Test failed");

