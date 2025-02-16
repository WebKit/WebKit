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
var BUGNUMBER = 562446;
var summary = 'ES5: Array.prototype.toString';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o;

o = { join: function() { assert.sameValue(arguments.length, 0); return "ohai"; } };
assert.sameValue(Array.prototype.toString.call(o), "ohai");

o = {};
assert.sameValue(Array.prototype.toString.call(o), "[object Object]");

Array.prototype.join = function() { return "kthxbai"; };
assert.sameValue(Array.prototype.toString.call([]), "kthxbai");

o = { join: 17 };
assert.sameValue(Array.prototype.toString.call(o), "[object Object]");

o = { get join() { throw 42; } };
try
{
  var str = Array.prototype.toString.call(o);
  assert.sameValue(true, false,
           "expected an exception calling [].toString on an object with a " +
           "join getter that throws, got " + str + " instead");
}
catch (e)
{
  assert.sameValue(e, 42,
           "expected thrown e === 42 when calling [].toString on an object " +
           "with a join getter that throws, got " + e);
}

/******************************************************************************/

print("All tests passed!");
