/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Date-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function throwsRangeError(t) {
    try {
        var date = new Date();
        date.setTime(t);
        var r = date.toISOString();
        throw new Error("toISOString didn't throw, instead returned " + r);
    } catch (err) {
        assert.sameValue(err instanceof RangeError, true, 'wrong error: ' + err);
        return;
    }
    assert.sameValue(0, 1, 'not good, nyan, nyan');
}

throwsRangeError(Infinity);
throwsRangeError(-Infinity);
throwsRangeError(NaN);

