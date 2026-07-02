function test() {

var prop = Object.getOwnPropertyDescriptor(Set, Symbol.species);
return 'get' in prop && Set[Symbol.species] === Set;
      
}

if (!test())
    throw new Error("Test failed");

