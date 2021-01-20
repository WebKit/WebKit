function test() {

'use strict';
const bar = 123;
{ const bar = 456; }
return bar === 123;
      
}

if (!test())
    throw new Error("Test failed");

