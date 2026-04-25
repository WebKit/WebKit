function test() {

return typeof WeakSet.prototype.delete === "function";
      
}

if (!test())
    throw new Error("Test failed");

