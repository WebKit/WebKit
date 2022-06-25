function test() {

return [...[1, 2, 3]][2] === 3;
      
}

if (!test())
    throw new Error("Test failed");

