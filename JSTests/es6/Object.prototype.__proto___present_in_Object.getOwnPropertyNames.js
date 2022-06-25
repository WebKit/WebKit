function test() {

return Object.getOwnPropertyNames(Object.prototype).indexOf('__proto__') > -1;
      
}

if (!test())
    throw new Error("Test failed");

