function test() {

return typeof RegExp.prototype[Symbol.match] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

