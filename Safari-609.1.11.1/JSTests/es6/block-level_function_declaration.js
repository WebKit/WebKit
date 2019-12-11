function test() {

'use strict';
function f() { return 1; }
{
  function f() { return 2; }
}
return f() === 1;
  
}

if (!test())
    throw new Error("Test failed");

