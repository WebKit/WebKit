function test() {

return typeof Set.prototype.values === "function";
      
}

if (!test())
    throw new Error("Test failed");

