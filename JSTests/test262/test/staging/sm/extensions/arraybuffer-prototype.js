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
var BUGNUMBER = 665961;
var summary =
  "ArrayBuffer cannot access properties defined on the prototype chain.";
print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

ArrayBuffer.prototype.prop = "on prototype";
var b = new ArrayBuffer([]);
assert.sameValue(b.prop, "on prototype");

var c = new ArrayBuffer([]);
assert.sameValue(c.prop, "on prototype");
c.prop = "direct";
assert.sameValue(c.prop, "direct");

assert.sameValue(ArrayBuffer.prototype.prop, "on prototype");
assert.sameValue(new ArrayBuffer([]).prop, "on prototype");

assert.sameValue(c.nonexistent, undefined);

