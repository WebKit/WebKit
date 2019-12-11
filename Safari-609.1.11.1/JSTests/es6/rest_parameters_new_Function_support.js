function test() {

return new Function("a", "...b",
  "return b instanceof Array && a+b === 'foobar,baz';"
)('foo','bar','baz');
      
}

if (!test())
    throw new Error("Test failed");

