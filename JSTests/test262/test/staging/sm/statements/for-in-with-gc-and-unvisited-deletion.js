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
var gTestfile = "for-in-with-gc-and-unvisited-deletion.js";
var BUGNUMBER = 1462939;
var summary =
  "Don't mishandle deletion of a property from the internal iterator " +
  "created for a for-in loop, when a gc occurs just after it";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function testOneDeletion()
{
  var o = {
    p: 1,
    r: 3,
    s: 4,
  };

  for (var i in o)
  {
    $262.gc();
    delete o.s;
  }
}
testOneDeletion();

function testTwoDeletions()
{
  var o = {
    p: 1,
    r: 3,
    s: 4,
    t: 5,
  };

  for (var i in o)
  {
    $262.gc();
    delete o.t;
    delete o.s;
  }
}
testTwoDeletions();

function testThreeDeletions()
{
  var o = {
    p: 1,
    r: 3,
    s: 4,
    t: 5,
    x: 7,
  };

  for (var i in o)
  {
    $262.gc();
    delete o.x;
    delete o.t;
    delete o.s;
  }
}
testThreeDeletions();

/******************************************************************************/

print("Tests complete");
