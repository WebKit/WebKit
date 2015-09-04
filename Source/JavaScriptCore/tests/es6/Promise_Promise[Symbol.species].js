function test() {

var prop = Object.getOwnPropertyDescriptor(Promise, Symbol.species);
return 'get' in prop && Promise[Symbol.species] === Promise;
      
}

if (!test())
    throw new Error("Test failed");

