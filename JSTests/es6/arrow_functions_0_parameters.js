function test() {

return (() => 5)() === 5;
      
}

if (!test())
    throw new Error("Test failed");

