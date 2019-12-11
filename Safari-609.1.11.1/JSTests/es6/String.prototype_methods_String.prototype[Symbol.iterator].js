function test() {

return typeof String.prototype[Symbol.iterator] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

