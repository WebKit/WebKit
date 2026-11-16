function test() {

return typeof Set.prototype.delete === "function";
      
}

if (!test())
    throw new Error("Test failed");

