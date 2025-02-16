// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Ensure that TypedArrays throw when attempting to sort a detached ArrayBuffer
if (typeof $262.detachArrayBuffer === "function") {
    assertThrowsInstanceOf(() => {
        let buffer = new ArrayBuffer(32);
        let array  = new Int32Array(buffer);
        $262.detachArrayBuffer(buffer);
        array.sort();
    }, TypeError);
}

// Ensure detaching buffer in comparator doesn't throw an error.
if (typeof $262.detachArrayBuffer === "function") {
    let detached = false;
    let ta = new Int32Array(3);
    ta.sort(function(a, b) {
        if (!detached) {
            detached = true;
            $262.detachArrayBuffer(ta.buffer);
        }
        return a - b;
    });
    assert.sameValue(detached, true);
}

// Ensure detachment check doesn't choke on wrapped typed array.
if (typeof createNewGlobal === "function") {
    let ta = new Int32Array(3);
    let otherGlobal = createNewGlobal();
    otherGlobal.Int32Array.prototype.sort.call(ta, function(a, b) {
        return a - b;
    });
}

// Ensure detaching buffer in comparator doesn't throw an error when the typed array is wrapped.
if (typeof createNewGlobal === "function" && typeof $262.detachArrayBuffer === "function") {
    let detached = false;
    let ta = new Int32Array(3);
    let otherGlobal = createNewGlobal();
    otherGlobal.Int32Array.prototype.sort.call(ta, function(a,b) {
        if (!detached) {
            detached = true;
            $262.detachArrayBuffer(ta.buffer);
        }
        return a - b;
    });
    assert.sameValue(detached, true);
}

// Ensure that TypedArray.prototype.sort will not sort non-TypedArrays
assertThrowsInstanceOf(() => {
    let array = [4, 3, 2, 1];
    Int32Array.prototype.sort.call(array);
}, TypeError);

assertThrowsInstanceOf(() => {
    Int32Array.prototype.sort.call({a: 1, b: 2});
}, TypeError);

assertThrowsInstanceOf(() => {
    Int32Array.prototype.sort.call(Int32Array.prototype);
}, TypeError);

assertThrowsInstanceOf(() => {
    let buf = new ArrayBuffer(32);
    Int32Array.prototype.sort.call(buf);
}, TypeError);

// Ensure that comparator errors are propagataed
function badComparator(x, y) {
    if (x == 99 && y == 99)
        throw new TypeError;
    return x - y;
}

assertThrowsInstanceOf(() => {
    let array = new Uint8Array([99, 99, 99, 99]);
    array.sort(badComparator);
}, TypeError);

assertThrowsInstanceOf(() => {
    let array = new Uint8Array([1, 99, 2, 99]);
    array.sort(badComparator);
}, TypeError);


