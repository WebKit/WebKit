function test() {

return typeof String.prototype.endsWith === 'function'
  && "foobar".endsWith("bar");
      
}

if (!test())
    throw new Error("Test failed");

