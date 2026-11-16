function test() {

return typeof Set.prototype.forEach === "function";
      
}

if (!test())
    throw new Error("Test failed");

