function test() {

var iterable = (function*(){ yield 1; yield 2; yield 3; }());
return Math.max(...iterable) === 3;
      
}

if (!test())
    throw new Error("Test failed");

