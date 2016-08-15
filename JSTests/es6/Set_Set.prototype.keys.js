function test() {

return typeof Set.prototype.keys === "function";
      
}

if (!test())
    throw new Error("Test failed");

