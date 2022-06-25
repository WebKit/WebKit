function test() {

var obj = {};
Reflect.preventExtensions(obj);
return !Object.isExtensible(obj);
      
}

if (!test())
    throw new Error("Test failed");

