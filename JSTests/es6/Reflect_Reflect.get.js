function test() {

return Reflect.get({ qux: 987 }, "qux") === 987;
      
}

if (!test())
    throw new Error("Test failed");

