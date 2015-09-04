function test() {

return typeof Array.prototype.entries === 'function';
      
}

if (!test())
    throw new Error("Test failed");

