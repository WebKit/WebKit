function test() {

return Reflect.has({ qux: 987 }, "qux");
      
}

if (!test())
    throw new Error("Test failed");

