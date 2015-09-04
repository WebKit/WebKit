function test() {

return typeof Symbol() === "symbol";
      
}

if (!test())
    throw new Error("Test failed");

