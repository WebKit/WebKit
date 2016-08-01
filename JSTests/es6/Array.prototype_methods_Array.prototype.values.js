function test() {

return typeof Array.prototype.values === 'function';
      
}

if (!test())
    throw new Error("Test failed");

