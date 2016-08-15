function test() {

return typeof Map.prototype.clear === "function";
      
}

if (!test())
    throw new Error("Test failed");

