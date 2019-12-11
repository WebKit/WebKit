function test() {

return Object.prototype.hasOwnProperty('__proto__');
      
}

if (!test())
    throw new Error("Test failed");

