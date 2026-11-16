function test() {

return typeof ArrayBuffer[Symbol.species] === 'function';
      
}

if (!test())
    throw new Error("Test failed");

