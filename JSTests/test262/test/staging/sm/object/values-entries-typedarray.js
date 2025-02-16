// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js, compareArray.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function assertSameEntries(actual, expected) {
    assert.sameValue(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        assert.compareArray(actual[i], expected[i]);
}

function throwsTypeError(fn) {
    try {
        fn();
    } catch (e) {
        assert.sameValue(e instanceof TypeError, true);
        return true;
    }
    return false;
}

// Non-standard: Accessing elements of detached array buffers should throw, but
// this is currently not implemented.
const ACCESS_ON_DETACHED_ARRAY_BUFFER_THROWS = (() => {
    let ta = new Int32Array(10);
    $262.detachArrayBuffer(ta.buffer);
    let throws = throwsTypeError(() => ta[0]);
    // Ensure [[Get]] and [[GetOwnProperty]] return consistent results.
    assert.sameValue(throwsTypeError(() => Object.getOwnPropertyDescriptor(ta, 0)), throws);
    return throws;
})();

function maybeThrowOnDetached(fn, returnValue) {
    if (ACCESS_ON_DETACHED_ARRAY_BUFFER_THROWS) {
        assertThrowsInstanceOf(fn, TypeError);
        return returnValue;
    }
    return fn();
}

// Ensure Object.keys/values/entries work correctly on typed arrays.
for (let len of [0, 1, 10]) {
    let array = new Array(len).fill(1);
    let ta = new Int32Array(array);

    assert.compareArray(Object.keys(ta), Object.keys(array));
    assert.compareArray(Object.values(ta), Object.values(array));
    assertSameEntries(Object.entries(ta), Object.entries(array));

    $262.detachArrayBuffer(ta.buffer);

    assert.compareArray(maybeThrowOnDetached(() => Object.keys(ta), []), []);
    assert.compareArray(maybeThrowOnDetached(() => Object.values(ta), []), []);
    assertSameEntries(maybeThrowOnDetached(() => Object.entries(ta), []), []);
}

