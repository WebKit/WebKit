function test() {

var obj = {};
Reflect.defineProperty(obj, "foo", { value: 123 });
return obj.foo === 123 &&
  Reflect.defineProperty(Object.freeze({}), "foo", { value: 123 }) === false;
      
}

if (!test())
    throw new Error("Test failed");

