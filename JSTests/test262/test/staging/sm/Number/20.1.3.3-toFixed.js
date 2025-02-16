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
assert.sameValue(Number.prototype.toFixed.call(-Infinity), "-Infinity");
assert.sameValue(Number.prototype.toFixed.call(Infinity), "Infinity");
assert.sameValue(Number.prototype.toFixed.call(NaN), "NaN");

assertThrowsInstanceOf(() => Number.prototype.toFixed.call(-Infinity, 555), RangeError);
assertThrowsInstanceOf(() => Number.prototype.toFixed.call(Infinity, 555), RangeError);
assertThrowsInstanceOf(() => Number.prototype.toFixed.call(NaN, 555), RangeError);

assertThrowsInstanceOf(() => Number.prototype.toFixed.call("Hello"), TypeError);

