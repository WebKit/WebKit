function test() {

return typeof Set.prototype[Symbol.iterator] === "function";
      
}

if (!test())
    throw new Error("Test failed");

