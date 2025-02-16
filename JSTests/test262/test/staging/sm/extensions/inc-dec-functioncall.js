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
//-----------------------------------------------------------------------------
var BUGNUMBER = 609756;
var summary =
  "Perform ToNumber on the result of the |fun()| in |fun()++| before " +
  "throwing";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var hadSideEffect;

function f()
{
  return { valueOf: function() { hadSideEffect = true; return 0; } };
}

hadSideEffect = false;
assertThrowsInstanceOf(function() { f()++; }, ReferenceError);
assert.sameValue(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      f()++;
  }
}, ReferenceError);
assert.sameValue(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { f()--; }, ReferenceError);
assert.sameValue(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      f()--;
  }
}, ReferenceError);
assert.sameValue(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { ++f(); }, ReferenceError);
assert.sameValue(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      ++f();
  }
}, ReferenceError);
assert.sameValue(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { --f(); }, ReferenceError);
assert.sameValue(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      --f();
  }
}, ReferenceError);
assert.sameValue(hadSideEffect, true);


/******************************************************************************/

print("Tests complete");
