function test() {

var {a = 1, b = 0, z:c = 3} = {b:2, z:undefined};
var [d = 0, e = 5, f = 6] = [4,,undefined];
return a === 1 && b === 2 && c === 3
  && d === 4 && e === 5 && f === 6;
      
}

if (!test())
    throw new Error("Test failed");

