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
var gTestfile = "DataView-set-arguments-detaching.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 991981;
var summary =
  "DataView.prototype.set* methods shouldn't misbehave horribly if " +
  "index-argument conversion detaches the ArrayBuffer being modified";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testIndex()
{
  var ab = new ArrayBuffer(0x1000);

  var dv = new DataView(ab);

  var start =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        $262.gc();
        return 0xFFF;
      }
    };

  var ok = false;
  try
  {
    dv.setUint8(start, 0x42);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "should have thrown");
  assert.sameValue(ab.byteLength, 0, "should have been detached correctly");
}
testIndex();

function testValue()
{
  var ab = new ArrayBuffer(0x100000);

  var dv = new DataView(ab);

  var value =
    {
      valueOf: function()
      {
        $262.detachArrayBuffer(ab);
        $262.gc();
        return 0x42;
      }
    };

  var ok = false;
  try
  {
    dv.setUint8(0xFFFFF, value);
  }
  catch (e)
  {
    ok = true;
  }
  assert.sameValue(ok, true, "should have thrown");
  assert.sameValue(ab.byteLength, 0, "should have been detached correctly");
}
testValue();

/******************************************************************************/

print("Tests complete");
