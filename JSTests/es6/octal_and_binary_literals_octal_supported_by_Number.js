function test() {

return Number('0o1') === 1;
      
}

if (!test())
    throw new Error("Test failed");

