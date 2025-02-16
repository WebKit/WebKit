/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 441477-01;
var summary = '';
var actual = 'No Exception';
var expect = 'No Exception';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    for (i = 0; i < 5;) 
    { 
      if (i > 5) 
        throw "bad"; 
      i++; 
      continue; 
    }
  }
  catch(ex)
  {
    actual = ex + '';
  }
  assert.sameValue(expect, actual, summary);
}
