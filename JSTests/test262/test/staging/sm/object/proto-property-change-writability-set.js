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
//-----------------------------------------------------------------------------
var BUGNUMBER = 713944;
var summary =
  "Don't assert anything about a shape from the property cache until it's " +
  "known the cache entry matches";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var accDesc = { set: function() {} };
var dataDesc = { value: 3 };

function f()
{
  propertyIsEnumerable = {};
}
function g()
{
  propertyIsEnumerable = {};
}

Object.defineProperty(Object.prototype, "propertyIsEnumerable", accDesc);
f();
Object.defineProperty(Object.prototype, "propertyIsEnumerable", dataDesc);
assert.sameValue(propertyIsEnumerable, 3);
f();
assert.sameValue(propertyIsEnumerable, 3);
g();
assert.sameValue(propertyIsEnumerable, 3);



var a = { p1: 1, p2: 2 };
var b = Object.create(a);
Object.defineProperty(a, "p1", {set: function () {}});
for (var i = 0; i < 2; i++)
{
  b.p1 = {};
  Object.defineProperty(a, "p1", {value: 3});
}
assert.sameValue(b.p1, 3);
assert.sameValue(a.p1, 3);

