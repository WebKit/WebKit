function test() {

return typeof RegExp.prototype[Symbol.replace] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

