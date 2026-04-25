function test() {

return Object.getOwnPropertyDescriptor('a', 'foo') === undefined;
      
}

if (!test())
    throw new Error("Test failed");

