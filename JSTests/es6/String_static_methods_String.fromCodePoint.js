function test() {

return typeof String.fromCodePoint === 'function';
      
}

if (!test())
    throw new Error("Test failed");

