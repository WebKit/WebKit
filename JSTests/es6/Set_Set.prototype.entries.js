function test() {

return typeof Set.prototype.entries === "function";
      
}

if (!test())
    throw new Error("Test failed");

