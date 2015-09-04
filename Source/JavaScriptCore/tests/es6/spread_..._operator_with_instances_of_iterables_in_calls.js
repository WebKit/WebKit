function test() {

var iterable = global.__createIterableObject([1, 2, 3]);
return Math.max(...Object.create(iterable)) === 3;
      
}

if (!test())
    throw new Error("Test failed");

