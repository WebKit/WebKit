function test() {

var descriptor = Object.getOwnPropertyDescriptor(function f(){},"name");
return descriptor.enumerable   === false &&
       descriptor.writable     === false &&
       descriptor.configurable === true;
      
}

if (!test())
    throw new Error("Test failed");

