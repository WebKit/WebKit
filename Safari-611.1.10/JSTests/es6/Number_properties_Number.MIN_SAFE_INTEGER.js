function test() {

return typeof Number.MIN_SAFE_INTEGER === 'number';
      
}

if (!test())
    throw new Error("Test failed");

