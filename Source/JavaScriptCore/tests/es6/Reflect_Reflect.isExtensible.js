function test() {

return Reflect.isExtensible({}) &&
  !Reflect.isExtensible(Object.preventExtensions({}));
      
}

if (!test())
    throw new Error("Test failed");

