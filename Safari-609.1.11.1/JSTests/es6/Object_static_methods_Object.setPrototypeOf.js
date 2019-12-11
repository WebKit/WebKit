function test() {

return Object.setPrototypeOf({}, Array.prototype) instanceof Array;
      
}

if (!test())
    throw new Error("Test failed");

