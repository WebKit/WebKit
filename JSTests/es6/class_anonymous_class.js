function test() {

return typeof class {} === "function";
      
}

if (!test())
    throw new Error("Test failed");

