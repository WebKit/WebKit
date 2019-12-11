function test() {

return Reflect.getPrototypeOf([]) === Array.prototype;
      
}

if (!test())
    throw new Error("Test failed");

