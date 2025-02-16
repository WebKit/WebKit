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
/**
 *  File Name:          regress-10278.js
 *  Reference:          https://bugzilla.mozilla.org/show_bug.cgi?id=10278
 *  Description:        Function declarations do not need to be separated
 *                      by semi-colon if they occur on the same line.
 *  Author:             bob@bclary.com
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 10278;
var summary = 'Function declarations do not need to be separated by semi-colon';
var actual;
var expect;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'pass';
  try
  {
    eval("function f(){}function g(){}");
    actual = "pass";
    printStatus('no exception thrown');
  }
  catch ( e )
  {
    actual = "fail";
    printStatus('exception ' + e.toString() + ' thrown');
  }

  assert.sameValue(expect, actual, summary);
}
