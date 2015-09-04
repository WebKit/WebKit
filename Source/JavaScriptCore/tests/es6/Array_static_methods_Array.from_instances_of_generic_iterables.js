function test() {

var iterable = global.__createIterableObject([1, 2, 3]);
return Array.from(Object.create(iterable)) + '' === "1,2,3";
      
}

if (!test())
    throw new Error("Test failed");

