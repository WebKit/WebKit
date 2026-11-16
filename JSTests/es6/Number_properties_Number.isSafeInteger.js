function test() {

return typeof Number.isSafeInteger === 'function';
      
}

if (!test())
    throw new Error("Test failed");

