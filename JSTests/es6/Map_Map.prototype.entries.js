function test() {

return typeof Map.prototype.entries === "function";
      
}

if (!test())
    throw new Error("Test failed");

