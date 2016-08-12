function test() {

var [a,] = [1];
return a === 1;
      
}

if (!test())
    throw new Error("Test failed");

