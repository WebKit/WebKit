function test() {

return typeof Set.prototype.clear === "function";
      
}

if (!test())
    throw new Error("Test failed");

