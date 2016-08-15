function test() {

return typeof Number.isNaN === 'function';
      
}

if (!test())
    throw new Error("Test failed");

