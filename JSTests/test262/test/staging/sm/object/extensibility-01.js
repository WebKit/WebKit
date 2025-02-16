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
var gTestfile = '15.2.3.10-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 492849;
var summary = 'ES5: Implement Object.preventExtensions, Object.isExtensible';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function trySetProperty(o, p, v, strict)
{
  function strictSetProperty()
  {
    "use strict";
    o[p] = v;
  }

  function setProperty()
  {
    o[p] = v;
  }

  assert.sameValue(Object.prototype.hasOwnProperty.call(o, p), false);

  try
  {
    if (strict)
      strictSetProperty();
    else
      setProperty();
    if (o[p] === v)
      return "set";
    if (p in o)
      return "set-converted";
    return "swallowed";
  }
  catch (e)
  {
    return "throw";
  }
}

function tryDefineProperty(o, p, v)
{
  assert.sameValue(Object.prototype.hasOwnProperty.call(o, p), false);

  try
  {
    Object.defineProperty(o, p, { value: v });
    if (o[p] === v)
      return "set";
    if (p in o)
      return "set-converted";
    return "swallowed";
  }
  catch (e)
  {
    return "throw";
  }
}

assert.sameValue(typeof Object.preventExtensions, "function");
assert.sameValue(Object.preventExtensions.length, 1);

var slowArray = [1, 2, 3];
slowArray.slow = 5;
var objs =
  [{}, { 1: 2 }, { a: 3 }, [], [1], [, 1], slowArray, function a(){}, /a/];

for (var i = 0, sz = objs.length; i < sz; i++)
{
  var o = objs[i];
  assert.sameValue(Object.isExtensible(o), true, "object " + i + " not extensible?");

  var o2 = Object.preventExtensions(o);
  assert.sameValue(o, o2);

  assert.sameValue(Object.isExtensible(o), false, "object " + i + " is extensible?");

  assert.sameValue(trySetProperty(o, "baz", 17, true), "throw",
           "unexpected behavior for strict-mode property-addition to " +
           "object " + i);
  assert.sameValue(trySetProperty(o, "baz", 17, false), "swallowed",
           "unexpected behavior for property-addition to object " + i);

  assert.sameValue(tryDefineProperty(o, "baz", 17), "throw",
           "unexpected behavior for new property definition on object " + i);
}

/******************************************************************************/

print("All tests passed!");
