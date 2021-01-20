function test() {

return Object.getPrototypeOf('a').constructor === String;
      
}

if (!test())
    throw new Error("Test failed");

