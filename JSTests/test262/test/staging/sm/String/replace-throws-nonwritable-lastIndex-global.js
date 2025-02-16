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
  "String.prototype.replace should throw when called with a global RegExp " +
  "whose .lastIndex is non-writable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = '0x2x4x6x8';

// First time with .lastIndex === 0, replacing to ''

var p1 = /x/g;
Object.defineProperty(p1, "lastIndex", { writable: false });

try
{
  s.replace(p1, '');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p1.lastIndex, 0);
}

// Second time with .lastIndex !== 0, replacing to ''

var p2 = /x/g;
Object.defineProperty(p2, "lastIndex", { writable: false, value: 3 });

try
{
  s.replace(p2, '');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p2.lastIndex, 3);
}

// Third time with .lastIndex === 0, replacing to 'y'

var p3 = /x/g;
Object.defineProperty(p3, "lastIndex", { writable: false });

try
{
  s.replace(p3, 'y');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p3.lastIndex, 0);
}

// Fourth time with .lastIndex !== 0, replacing to 'y'

var p4 = /x/g;
Object.defineProperty(p4, "lastIndex", { writable: false, value: 3 });

try
{
  s.replace(p4, '');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p4.lastIndex, 3);
}

// Fifth time with .lastIndex === 0, replacing to 'y', but no match

var p5 = /q/g;
Object.defineProperty(p5, "lastIndex", { writable: false });

try
{
  s.replace(p5, 'y');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p5.lastIndex, 0);
}

// Sixth time with .lastIndex !== 0, replacing to 'y', but no match

var p6 = /q/g;
Object.defineProperty(p6, "lastIndex", { writable: false, value: 3 });

try
{
  s.replace(p6, '');
  throw "didn't throw";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown a TypeError, instead got: " + e);
  assert.sameValue(p6.lastIndex, 3);
}

/******************************************************************************/

print("Tests complete");
