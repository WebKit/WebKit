function test() {

var result = "";
var iterable = global.__createIterableObject([1, 2, 3]);
for (var item of iterable) {
  result += item;
}
return result === "123";
      
}

if (!test())
    throw new Error("Test failed");

