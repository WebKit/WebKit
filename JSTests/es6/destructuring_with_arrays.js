function test() {

var [a, , [b], c] = [5, null, [6]];
var d, e;
[d,e] = [7,8];
return a === 5 && b === 6 && c === undefined
  && d === 7 && e === 8;
      
}

if (!test())
    throw new Error("Test failed");

