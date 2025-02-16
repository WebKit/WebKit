/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 501739;
var summary =
  "String.prototype.match should throw when called with a global RegExp " +
  "whose .lastIndex is non-writable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = '0x2x4x6x8';

// First time with .lastIndex === 0

var p1 = /x/g;
Object.defineProperty(p1, "lastIndex", { writable: false });

try
{
  s.match(p1);
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
}

// Second time with .lastIndex !== 0

var p2 = /x/g;
Object.defineProperty(p2, "lastIndex", { writable: false, value: 3 });

try
{
  s.match(p2);
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
}

// Third time with .lastIndex === 0, no matches

var p3 = /q/g;
Object.defineProperty(p3, "lastIndex", { writable: false });

try
{
  s.match(p3);
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
}

// Fourth time with .lastIndex !== 0, no matches

var p4 = /q/g;
Object.defineProperty(p4, "lastIndex", { writable: false, value: 3 });

try
{
  s.match(p4);
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
}

/******************************************************************************/

print("Tests complete");
