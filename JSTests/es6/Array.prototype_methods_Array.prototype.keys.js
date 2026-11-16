function test() {

return typeof Array.prototype.keys === 'function';
      
}

if (!test())
    throw new Error("Test failed");

