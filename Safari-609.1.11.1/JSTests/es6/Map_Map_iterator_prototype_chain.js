function test() {

// Iterator instance
var iterator = new Map()[Symbol.iterator]();
// %MapIteratorPrototype%
var proto1 = Object.getPrototypeOf(iterator);
// %IteratorPrototype%
var proto2 = Object.getPrototypeOf(proto1);

return proto2.hasOwnProperty(Symbol.iterator) &&
  !proto1    .hasOwnProperty(Symbol.iterator) &&
  !iterator  .hasOwnProperty(Symbol.iterator) &&
  iterator[Symbol.iterator]() === iterator;
      
}

if (!test())
    throw new Error("Test failed");

