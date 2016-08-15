function test() {

var iterable = (function*(){ yield 1; yield 2; yield 3; }());
return Array.from(iterable) + '' === "1,2,3";
      
}

if (!test())
    throw new Error("Test failed");

