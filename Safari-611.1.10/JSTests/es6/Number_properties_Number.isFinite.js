function test() {

return typeof Number.isFinite === 'function';
      
}

if (!test())
    throw new Error("Test failed");

