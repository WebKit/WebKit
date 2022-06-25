function test() {

return 0b10 === 2 && 0B10 === 2;
      
}

if (!test())
    throw new Error("Test failed");

