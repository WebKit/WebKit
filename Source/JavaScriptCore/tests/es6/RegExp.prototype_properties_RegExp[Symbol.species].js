function test() {

var prop = Object.getOwnPropertyDescriptor(RegExp, Symbol.species);
return 'get' in prop && RegExp[Symbol.species] === RegExp;
      
}

if (!test())
    throw new Error("Test failed");

