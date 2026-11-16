function test() {

return typeof RegExp.prototype.compile === 'function';
  
}

if (!test())
    throw new Error("Test failed");

