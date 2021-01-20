function test() {

return Object.isFrozen('a') === true;
      
}

if (!test())
    throw new Error("Test failed");

