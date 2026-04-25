function test() {

var a,b,c,d;
[a,b] = [c,d] = [1,2];
return a === 1 && b === 2 && c === 1 && d === 2;
      
}

if (!test())
    throw new Error("Test failed");

