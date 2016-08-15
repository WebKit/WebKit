function test() {

return ({ y() { return 2; } }).y() === 2;
      
}

if (!test())
    throw new Error("Test failed");

