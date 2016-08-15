function test() {

'use strict';
return this === undefined && ({ a:1, a:1 }).a === 1;
      
}

if (!test())
    throw new Error("Test failed");

