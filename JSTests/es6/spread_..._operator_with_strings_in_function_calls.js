function test() {

return Math.max(..."1234") === 4;
      
}

if (!test())
    throw new Error("Test failed");

