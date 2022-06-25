function test() {

return typeof Array.prototype[Symbol.iterator] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

