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
var gTestfile = "TypedArray-subarray-arguments-detaching.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "%TypedArray.prototype.subarray shouldn't misbehave horribly if " +
  "index-argument conversion detaches the underlying ArrayBuffer";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testBegin()
{
  var ab = new ArrayBuffer(0x1000);

  var begin =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        return 0x800;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(begin);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "start weirdness should have thrown");
  assert.sameValue(ab.byteLength, 0, "detaching should work for start weirdness");
}
testBegin();

function testBeginWithEnd()
{
  var ab = new ArrayBuffer(0x1000);

  var begin =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        return 0x800;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(begin, 0x1000);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "start weirdness should have thrown");
  assert.sameValue(ab.byteLength, 0, "detaching should work for start weirdness");
}
testBeginWithEnd();

function testEnd()
{
  var ab = new ArrayBuffer(0x1000);

  var end =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        return 0x1000;
      }
    };

  var ta = new Uint8Array(ab);

  var ok = false;
  try
  {
    ta.subarray(0x800, end);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "start weirdness should have thrown");
  assert.sameValue(ab.byteLength, 0, "detaching should work for start weirdness");
}
testEnd();

/******************************************************************************/

print("Tests complete");
