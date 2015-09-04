function test() {

class C { foo(){} };
return (new C).foo.name === "foo";
      
}

if (!test())
    throw new Error("Test failed");

