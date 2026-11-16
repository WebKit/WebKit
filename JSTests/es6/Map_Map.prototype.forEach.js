function test() {

return typeof Map.prototype.forEach === "function";
      
}

if (!test())
    throw new Error("Test failed");

