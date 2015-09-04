function test() {

return typeof RegExp.prototype[Symbol.split] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

