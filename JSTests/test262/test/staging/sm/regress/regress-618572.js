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
var BUGNUMBER = 618572;
var summary = 'Do not assert when ungetting a Unicode char sequence';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'SyntaxError';

  try
  {
    eval("var a\\0021 = 3;");
  }
  catch(ex)
  {
    actual = ex.constructor.name;
  }

  assert.sameValue(expect, actual, summary);
}
