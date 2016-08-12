function test() {

return typeof Map.prototype.delete === "function";
      
}

if (!test())
    throw new Error("Test failed");

