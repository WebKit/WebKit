function test() {

return typeof Array.prototype.find === 'function';
      
}

if (!test())
    throw new Error("Test failed");

