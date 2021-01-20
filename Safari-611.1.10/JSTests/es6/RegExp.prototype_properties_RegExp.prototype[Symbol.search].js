function test() {

return typeof RegExp.prototype[Symbol.search] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

