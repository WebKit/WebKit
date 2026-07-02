function test() {

var prop = Object.getOwnPropertyDescriptor(Map, Symbol.species);
return 'get' in prop && Map[Symbol.species] === Map;
      
}

if (!test())
    throw new Error("Test failed");

