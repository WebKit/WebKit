function test() {

return typeof String.raw === 'function';
      
}

if (!test())
    throw new Error("Test failed");

