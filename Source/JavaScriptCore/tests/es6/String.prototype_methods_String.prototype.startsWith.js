function test() {

return typeof String.prototype.startsWith === 'function'
  && "foobar".startsWith("foo");
      
}

if (!test())
    throw new Error("Test failed");

