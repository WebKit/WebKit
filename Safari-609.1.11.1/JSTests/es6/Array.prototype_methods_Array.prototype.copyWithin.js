function test() {

return typeof Array.prototype.copyWithin === 'function';
      
}

if (!test())
    throw new Error("Test failed");

