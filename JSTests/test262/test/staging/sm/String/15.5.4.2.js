/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call(42)'), true);
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call(true)'), true);
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call({})'), true);
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call(null)'), true);
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call([])'), true);
assert.sameValue(raisesException(TypeError)('String.prototype.toString.call(undefined)'), true);
assert.sameValue(completesNormally('String.prototype.toString.call("")'), true);

