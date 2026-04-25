function test() {

return typeof Number.EPSILON === 'number';
      
}

if (!test())
    throw new Error("Test failed");

