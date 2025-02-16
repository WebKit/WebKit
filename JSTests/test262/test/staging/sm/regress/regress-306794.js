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
var BUGNUMBER = 306794;
var summary = 'Do not assert: parsing foo getter';
var actual = 'No Assertion';
var expect = 'No Assertion';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
try
{
  eval('getter\n');
}
catch(e)
{
}

assert.sameValue(expect, actual, summary);
