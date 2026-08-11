function test() {

'use strict';
let bar = 123;
{ let bar = 456; }
return bar === 123;
      
}

if (!test())
    throw new Error("Test failed");

