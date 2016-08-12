function test() {

return (function (a, b = a) { return b === 5; }(5));
      
}

if (!test())
    throw new Error("Test failed");

