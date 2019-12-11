function test() {

return typeof Number.isInteger === 'function';
      
}

if (!test())
    throw new Error("Test failed");

