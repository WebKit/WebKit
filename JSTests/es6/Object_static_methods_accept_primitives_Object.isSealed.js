function test() {

return Object.isSealed('a') === true;
      
}

if (!test())
    throw new Error("Test failed");

