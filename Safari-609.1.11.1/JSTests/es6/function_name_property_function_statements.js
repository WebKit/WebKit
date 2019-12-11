function test() {

function foo(){};
return foo.name === 'foo' &&
  (function(){}).name === '';
      
}

if (!test())
    throw new Error("Test failed");

