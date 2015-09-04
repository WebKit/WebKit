function test() {

return typeof Array.of === 'function' &&
  Array.of(2)[0] === 2;
      
}

if (!test())
    throw new Error("Test failed");

