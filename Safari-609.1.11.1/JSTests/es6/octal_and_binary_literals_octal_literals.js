function test() {

return 0o10 === 8 && 0O10 === 8;
      
}

if (!test())
    throw new Error("Test failed");

