function test() {

return typeof Array.prototype.findIndex === 'function';
      
}

if (!test())
    throw new Error("Test failed");

