function test() {

var result = "";
var iterable = (function*(){ yield 1; yield 2; yield 3; }());
for (var item of iterable) {
  result += item;
}
return result === "123";
      
}

if (!test())
    throw new Error("Test failed");

