function test() {

var [a, b, c] = (function*(){ yield 1; yield 2; }());
var d, e;
[d, e] = (function*(){ yield 3; yield 4; }());
return a === 1 && b === 2 && c === undefined
  && d === 3 && e === 4;
      
}

if (!test())
    throw new Error("Test failed");

