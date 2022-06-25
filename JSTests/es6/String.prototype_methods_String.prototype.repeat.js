function test() {

return typeof String.prototype.repeat === 'function'
  && "foo".repeat(3) === "foofoofoo";
      
}

if (!test())
    throw new Error("Test failed");

