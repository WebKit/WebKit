/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'isPrototypeOf.js';
var BUGNUMBER = 619283;
var summary = "Object.prototype.isPrototypeOf";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowTypeError(fun)
{
  try
  {
    var r = fun();
    throw new Error("didn't throw TypeError, returned " + r);
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "didn't throw TypeError, got: " + e);
  }
}

var isPrototypeOf = Object.prototype.isPrototypeOf;

/*
 * 1. If V is not an Object, return false.
 */
assert.sameValue(isPrototypeOf(), false);
assert.sameValue(isPrototypeOf(1), false);
assert.sameValue(isPrototypeOf(Number.MAX_VALUE), false);
assert.sameValue(isPrototypeOf(NaN), false);
assert.sameValue(isPrototypeOf(""), false);
assert.sameValue(isPrototypeOf("sesquicentennial"), false);
assert.sameValue(isPrototypeOf(true), false);
assert.sameValue(isPrototypeOf(false), false);
assert.sameValue(isPrototypeOf(0.72), false);
assert.sameValue(isPrototypeOf(undefined), false);
assert.sameValue(isPrototypeOf(null), false);


/*
 * 2. Let O be the result of calling ToObject passing the this value as the
 *    argument.
 */
var protoGlobal = Object.create(this);
expectThrowTypeError(function() { isPrototypeOf.call(null, {}); });
expectThrowTypeError(function() { isPrototypeOf.call(undefined, {}); });
expectThrowTypeError(function() { isPrototypeOf({}); });
expectThrowTypeError(function() { isPrototypeOf.call(null, protoGlobal); });
expectThrowTypeError(function() { isPrototypeOf.call(undefined, protoGlobal); });
expectThrowTypeError(function() { isPrototypeOf(protoGlobal); });


/*
 * 3. Repeat
 */

/*
 * 3a. Let V be the value of the [[Prototype]] internal property of V.
 * 3b. If V is null, return false.
 */
assert.sameValue(Object.prototype.isPrototypeOf(Object.prototype), false);
assert.sameValue(String.prototype.isPrototypeOf({}), false);
assert.sameValue(Object.prototype.isPrototypeOf(Object.create(null)), false);

/* 3c. If O and V refer to the same object, return true. */
assert.sameValue(Object.prototype.isPrototypeOf({}), true);
assert.sameValue(this.isPrototypeOf(protoGlobal), true);
assert.sameValue(Object.prototype.isPrototypeOf({}), true);
assert.sameValue(Object.prototype.isPrototypeOf(new Number(17)), true);
assert.sameValue(Object.prototype.isPrototypeOf(function(){}), true);


/******************************************************************************/

print("All tests passed!");
