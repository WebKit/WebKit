function test() {

return typeof Number.MAX_SAFE_INTEGER === 'number';
      
}

if (!test())
    throw new Error("Test failed");

