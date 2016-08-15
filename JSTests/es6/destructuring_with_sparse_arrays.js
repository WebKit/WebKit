function test() {

var [a, b] = [,,];
return a === undefined && b === undefined;
      
}

if (!test())
    throw new Error("Test failed");

