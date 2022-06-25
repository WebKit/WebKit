function test() {

var obj = {};
Reflect.set(obj, "quux", 654);
return obj.quux === 654;
      
}

if (!test())
    throw new Error("Test failed");

