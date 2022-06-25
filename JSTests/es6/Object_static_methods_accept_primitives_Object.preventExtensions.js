function test() {

return Object.preventExtensions('a') === 'a';
      
}

if (!test())
    throw new Error("Test failed");

