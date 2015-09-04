function test() {

var obj = { baz: 789 };
var desc = Reflect.getOwnPropertyDescriptor(obj, "baz");
return desc.value === 789 &&
  desc.configurable && desc.writable && desc.enumerable;
      
}

if (!test())
    throw new Error("Test failed");

