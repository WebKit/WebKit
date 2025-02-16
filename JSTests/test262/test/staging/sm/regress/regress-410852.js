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
var BUGNUMBER = 410852;
var summary = 'Valgrind errors in jsemit.cpp';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  print('Note: You must run this test under valgrind to determine if it passes');

  try
  {
    eval('function(){if(t)');
  }
  catch(ex)
  {
    assert.sameValue(ex instanceof SyntaxError, true, "wrong error: " + ex);
  }

}
