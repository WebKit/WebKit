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
var BUGNUMBER = 657298;
var summary = 'Various quirks of setting array length properties to objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function invokeConversionTwice1()
{
  var count = 0;
  [].length = { valueOf: function() { count++; return 1; } };
  assert.sameValue(count, 2);
}
invokeConversionTwice1();

function invokeConversionTwice2()
{
  var count = 0;
  [].length = { toString: function() { count++; return 1; }, valueOf: null };
  assert.sameValue(count, 2);
}
invokeConversionTwice2();

function dontOverwriteError1()
{
  try
  {
    [].length = { valueOf: {}, toString: {} };
    throw new Error("didn't throw a TypeError");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "expected a TypeError running out of conversion options, got " + e);
  }
}
dontOverwriteError1();

function dontOverwriteError2()
{
  try
  {
    [].length = { valueOf: function() { throw "error"; } };
    throw new Error("didn't throw a TypeError");
  }
  catch (e)
  {
    assert.sameValue(e, "error", "expected 'error' from failed conversion, got " + e);
  }
}
dontOverwriteError2();

/******************************************************************************/

print("All tests passed!");
