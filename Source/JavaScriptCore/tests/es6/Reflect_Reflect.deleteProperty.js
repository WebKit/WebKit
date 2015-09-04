function test() {

var obj = { bar: 456 };
Reflect.deleteProperty(obj, "bar");
return !("bar" in obj);
      
}

if (!test())
    throw new Error("Test failed");

