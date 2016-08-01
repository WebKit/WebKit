function test() {

return Object.seal('a') === 'a';
      
}

if (!test())
    throw new Error("Test failed");

