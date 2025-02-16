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
var gTestfile = 'preventExtensions-idempotent.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 599459;
var summary = 'Object.preventExtensions should be idempotent';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj = {};
assert.sameValue(Object.preventExtensions(obj), obj);
assert.sameValue(Object.isExtensible(obj), false);
assert.sameValue(Object.preventExtensions(obj), obj);
assert.sameValue(Object.isExtensible(obj), false);

/******************************************************************************/

print("All tests passed!");
