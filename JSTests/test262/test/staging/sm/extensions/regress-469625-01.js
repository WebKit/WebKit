/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 469625;
var summary = 'TM: Array prototype and expression closures';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'TypeError: [].__proto__ is not a function';


  Array.prototype.__proto__ = function () { return 3; };

  try
  {
    [].__proto__();
  }
  catch(ex)
  {
    print(actual = ex + '');
  }

  assert.sameValue(expect, actual, summary);
}
