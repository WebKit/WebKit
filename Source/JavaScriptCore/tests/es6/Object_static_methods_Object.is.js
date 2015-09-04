function test() {

return typeof Object.is === 'function' &&
  Object.is(NaN, NaN) &&
 !Object.is(-0, 0);
      
}

if (!test())
    throw new Error("Test failed");

