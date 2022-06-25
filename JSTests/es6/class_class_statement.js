function test() {

class C {}
return typeof C === "function";
      
}

if (!test())
    throw new Error("Test failed");

