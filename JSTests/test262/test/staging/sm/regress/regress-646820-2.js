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
(function () {
    var obj = {prop: 1};
    var [x, {prop: y}] = [function () { return y; }, obj];
    assert.sameValue(y, 1);
    assert.sameValue(x(), 1);
})();

