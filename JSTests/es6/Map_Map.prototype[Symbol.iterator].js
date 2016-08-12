function test() {

return typeof Map.prototype[Symbol.iterator] === "function";
      
}

if (!test())
    throw new Error("Test failed");

