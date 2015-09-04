function test() {

return Object.freeze('a') === 'a';
      
}

if (!test())
    throw new Error("Test failed");

