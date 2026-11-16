function test() {

return typeof Map.prototype.keys === "function";
      
}

if (!test())
    throw new Error("Test failed");

