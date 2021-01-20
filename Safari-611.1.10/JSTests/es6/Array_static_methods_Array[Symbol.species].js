function test() {

var prop = Object.getOwnPropertyDescriptor(Array, Symbol.species);
return 'get' in prop && Array[Symbol.species] === Array;
      
}

if (!test())
    throw new Error("Test failed");

