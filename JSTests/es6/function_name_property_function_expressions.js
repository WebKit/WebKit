function test() {

return (function foo(){}).name === 'foo' &&
  (function(){}).name === '';
      
}

if (!test())
    throw new Error("Test failed");

