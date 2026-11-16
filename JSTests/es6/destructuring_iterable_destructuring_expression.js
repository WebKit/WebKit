function test() {

var a, b, iterable = [1,2];
return ([a, b] = iterable) === iterable;
      
}

if (!test())
    throw new Error("Test failed");

