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
var gTestfile = "ArrayBuffer-slice-arguments-detaching.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "ArrayBuffer.prototype.slice shouldn't misbehave horribly if " +
  "index-argument conversion detaches the ArrayBuffer being sliced";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testStart()
{
  var ab = new ArrayBuffer(0x1000);

  var start =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        $262.gc();
        return 0x800;
      }
    };

  var ok = false;
  try
  {
    ab.slice(start);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "start weirdness should have thrown");
  assert.sameValue(ab.byteLength, 0, "detaching should work for start weirdness");
}
testStart();

function testEnd()
{
  var ab = new ArrayBuffer(0x1000);

  var end =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        $262.gc();
        return 0x1000;
      }
    };

  var ok = false;
  try
  {
    ab.slice(0x800, end);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "byteLength weirdness should have thrown");
  assert.sameValue(ab.byteLength, 0, "detaching should work for byteLength weirdness");
}
testEnd();

/******************************************************************************/

print("Tests complete");
