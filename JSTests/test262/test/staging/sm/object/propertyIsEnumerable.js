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
var gTestfile = 'propertyIsEnumerable.js';
var BUGNUMBER = 619283;
var summary = "Object.prototype.propertyIsEnumerable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowError(errorCtor, fun)
{
  try
  {
    var r = fun();
    throw "didn't throw TypeError, returned " + r;
  }
  catch (e)
  {
    assert.sameValue(e instanceof errorCtor, true,
             "didn't throw " + errorCtor.prototype.name + ", got: " + e);
  }
}

function expectThrowTypeError(fun)
{
  expectThrowError(TypeError, fun);
}

function withToString(fun)
{
  return { toString: fun };
}

function withValueOf(fun)
{
  return { toString: null, valueOf: fun };
}

var propertyIsEnumerable = Object.prototype.propertyIsEnumerable;

/*
 * 1. Let P be ToString(V).
 */
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable(withToString(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { fahslkjdfhlkjdsl; }));
});

expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable(withValueOf(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { fahslkjdfhlkjdsl; }));
});

expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable(withToString(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { eval("}"); }));
});

expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable(withValueOf(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { eval("}"); }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withToString(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { [].length = -1; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withValueOf(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { [].length = -1; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withToString(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { [].length = 0.7; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withValueOf(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { [].length = 0.7; }));
});

/*
 * 2. Let O be the result of calling ToObject passing the this value as the
 *    argument.
 */
expectThrowTypeError(function() { propertyIsEnumerable("s"); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, "s"); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, "s"); });
expectThrowTypeError(function() { propertyIsEnumerable(true); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, true); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, true); });
expectThrowTypeError(function() { propertyIsEnumerable(NaN); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, NaN); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, NaN); });

expectThrowTypeError(function() { propertyIsEnumerable({}); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, {}); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, {}); });

/*
 * 3. Let desc be the result of calling the [[GetOwnProperty]] internal method
 *    of O passing P as the argument.
 * 4. If desc is undefined, return false.
 */
assert.sameValue(propertyIsEnumerable.call({}, "valueOf"), false);
assert.sameValue(propertyIsEnumerable.call({}, "toString"), false);
assert.sameValue(propertyIsEnumerable.call("s", 1), false);
assert.sameValue(propertyIsEnumerable.call({}, "dsfiodjfs"), false);
assert.sameValue(propertyIsEnumerable.call(true, "toString"), false);
assert.sameValue(propertyIsEnumerable.call({}, "__proto__"), false);

assert.sameValue(propertyIsEnumerable.call(Object, "getOwnPropertyDescriptor"), false);
assert.sameValue(propertyIsEnumerable.call(this, "expectThrowTypeError"), true);
assert.sameValue(propertyIsEnumerable.call("s", "length"), false);
assert.sameValue(propertyIsEnumerable.call("s", 0), true);
assert.sameValue(propertyIsEnumerable.call(Number, "MAX_VALUE"), false);
assert.sameValue(propertyIsEnumerable.call({ x: 9 }, "x"), true);
assert.sameValue(propertyIsEnumerable.call(function() { }, "prototype"), false);
assert.sameValue(propertyIsEnumerable.call(function() { }, "length"), false);
assert.sameValue(propertyIsEnumerable.call(function() { "use strict"; }, "caller"), false);

/******************************************************************************/

print("All tests passed!");
