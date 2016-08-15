function test() {

return typeof String.prototype.includes === 'function'
  && "foobar".includes("oba");
      
}

if (!test())
    throw new Error("Test failed");

