// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var buffer = new ArrayBuffer(2);
var view = new DataView(buffer);

function check(view) {
    for (let fun of ['getInt8', 'setInt8', 'getInt16', 'setInt16']) {
        assertThrowsInstanceOf(() => view[fun](-10), RangeError);
        assertThrowsInstanceOf(() => view[fun](-Infinity), RangeError);
        assertThrowsInstanceOf(() => view[fun](Infinity), RangeError);

        assertThrowsInstanceOf(() => view[fun](Math.pow(2, 53)), RangeError);
        assertThrowsInstanceOf(() => view[fun](Math.pow(2, 54)), RangeError);
    }
}

check(view);

for (let fun of ['getInt8', 'getInt16']) {
    assert.sameValue(view[fun](0), 0);
    assert.sameValue(view[fun](undefined), 0);
    assert.sameValue(view[fun](NaN), 0);
}

if ('$262.detachArrayBuffer' in this) {
    // ToIndex is called before detachment check, so we can tell the difference
    // between a ToIndex failure and a real out of bounds failure.
    $262.detachArrayBuffer(buffer);

    check(view);

    assertThrowsInstanceOf(() => view.getInt8(0), TypeError);
    assertThrowsInstanceOf(() => view.setInt8(0, 0), TypeError);
    assertThrowsInstanceOf(() => view.getInt8(Math.pow(2, 53) - 1), TypeError);
    assertThrowsInstanceOf(() => view.setInt8(Math.pow(2, 53) - 1, 0), TypeError);
}

