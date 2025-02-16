/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 578273;
var summary =
  "ES5: Properly detect cycles in JSON.stringify (throw TypeError, check for " +
  "cycles rather than imprecisely rely on recursion limits)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// objects

var count = 0;
var desc =
  {
    get: function() { count++; return obj; },
    enumerable: true,
    configurable: true
  };
var obj = Object.defineProperty({ p1: 0 }, "p2", desc);

try
{
  var str = JSON.stringify(obj);
  assert.sameValue(false, true, "should have thrown, got " + str);
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "wrong error type: " + e.constructor.name);
  assert.sameValue(count, 1,
           "cyclic data structures not detected immediately");
}

count = 0;
var obj2 = Object.defineProperty({}, "obj", desc);
try
{
  var str = JSON.stringify(obj2);
  assert.sameValue(false, true, "should have thrown, got " + str);
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "wrong error type: " + e.constructor.name);
  assert.sameValue(count, 2,
           "cyclic data structures not detected immediately");
}


// arrays

var count = 0;
var desc =
  {
    get: function() { count++; return arr; },
    enumerable: true,
    configurable: true
  };
var arr = Object.defineProperty([], "0", desc);

try
{
  var str = JSON.stringify(arr);
  assert.sameValue(false, true, "should have thrown, got " + str);
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "wrong error type: " + e.constructor.name);
  assert.sameValue(count, 1,
           "cyclic data structures not detected immediately");
}

count = 0;
var arr2 = Object.defineProperty([], "0", desc);
try
{
  var str = JSON.stringify(arr2);
  assert.sameValue(false, true, "should have thrown, got " + str);
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "wrong error type: " + e.constructor.name);
  assert.sameValue(count, 2,
           "cyclic data structures not detected immediately");
}

/******************************************************************************/

print("Tests complete");
