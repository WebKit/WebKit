function test() {

return typeof Map.prototype.values === "function";
      
}

if (!test())
    throw new Error("Test failed");

