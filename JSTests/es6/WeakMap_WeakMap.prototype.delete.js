function test() {

return typeof WeakMap.prototype.delete === "function";
      
}

if (!test())
    throw new Error("Test failed");

