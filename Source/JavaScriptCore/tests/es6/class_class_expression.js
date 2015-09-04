function test() {

return typeof class C {} === "function";
      
}

if (!test())
    throw new Error("Test failed");

