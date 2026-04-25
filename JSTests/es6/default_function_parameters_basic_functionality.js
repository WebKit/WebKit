function test() {

return (function (a = 1, b = 2) { return a === 3 && b === 2; }(3));
      
}

if (!test())
    throw new Error("Test failed");

