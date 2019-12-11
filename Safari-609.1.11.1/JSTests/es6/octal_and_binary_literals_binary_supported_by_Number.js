function test() {

return Number('0b1') === 1;
      
}

if (!test())
    throw new Error("Test failed");

