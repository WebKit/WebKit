function test() {

return Reflect.apply(Array.prototype.push, [1,2], [3,4,5]) === 5;
      
}

if (!test())
    throw new Error("Test failed");

