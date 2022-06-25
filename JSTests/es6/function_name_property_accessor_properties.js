function test() {

var o = { get foo(){}, set foo(x){} };
var descriptor = Object.getOwnPropertyDescriptor(o, "foo");
return descriptor.get.name === "get foo" &&
       descriptor.set.name === "set foo";
      
}

if (!test())
    throw new Error("Test failed");

