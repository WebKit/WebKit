function test() {

return Math.max(...[1, 2, 3]) === 3
      
}

if (!test())
    throw new Error("Test failed");

