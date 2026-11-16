function test() {

return typeof Array.prototype.fill === 'function';
      
}

if (!test())
    throw new Error("Test failed");

