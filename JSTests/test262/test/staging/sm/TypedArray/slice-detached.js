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
// Tests for detached ArrayBuffer checks in %TypedArray%.prototype.slice ( start, end ).

function* createTypedArrays(lengths = [0, 1, 4, 4096]) {
    // Test with eagerly created ArrayBuffer.
    for (let length of lengths) {
        let buffer = new ArrayBuffer(length * Int32Array.BYTES_PER_ELEMENT);
        let typedArray = new Int32Array(buffer);

        yield {typedArray, length, buffer() { return buffer; }};
    }

    // Test with lazily created ArrayBuffer.
    for (let length of lengths) {
        let typedArray = new Int32Array(length);

        yield {typedArray, length, buffer() { return typedArray.buffer; }};
    }
}

if (typeof $262.detachArrayBuffer === "function") {
    // ArrayBuffer is detached when entering slice().
    for (let {typedArray, buffer} of createTypedArrays()) {
        $262.detachArrayBuffer(buffer());
        assertThrowsInstanceOf(() => {
            typedArray.slice(0);
        }, TypeError, "ArrayBuffer is detached on function entry");
    }

    // ArrayBuffer is detached when computing ToInteger(start).
    for (let {typedArray, length, buffer} of createTypedArrays()) {
        let detached = false;
        let start = {
            valueOf() {
                assert.sameValue(detached, false);
                $262.detachArrayBuffer(buffer());
                assert.sameValue(detached, false);
                detached = true;
                return 0;
            }
        };

        // Doesn't throw an error when no bytes are copied.
        if (length === 0) {
            typedArray.slice(start);
        } else {
            assertThrowsInstanceOf(() => {
                typedArray.slice(start);
            }, TypeError, "ArrayBuffer is detached in ToInteger(start)");
        }
        assert.sameValue(detached, true, "$262.detachArrayBuffer was called");
    }

    // ArrayBuffer is detached when computing ToInteger(end).
    for (let {typedArray, length, buffer} of createTypedArrays()) {
        let detached = false;
        let end = {
            valueOf() {
                assert.sameValue(detached, false);
                $262.detachArrayBuffer(buffer());
                assert.sameValue(detached, false);
                detached = true;
                return length;
            }
        };

        // Doesn't throw an error when no bytes are copied.
        if (length === 0) {
            typedArray.slice(0, end);
        } else {
            assertThrowsInstanceOf(() => {
                typedArray.slice(0, end);
            }, TypeError, "ArrayBuffer is detached in ToInteger(end)");
        }
        assert.sameValue(detached, true, "$262.detachArrayBuffer was called");
    }

    // ArrayBuffer is detached in species constructor.
    for (let {typedArray, length, buffer} of createTypedArrays()) {
        let detached = false;
        typedArray.constructor = {
            [Symbol.species]: function(...args) {
                assert.sameValue(detached, false);
                $262.detachArrayBuffer(buffer());
                assert.sameValue(detached, false);
                detached = true;
                return new Int32Array(...args);
            }
        };

        // Doesn't throw an error when no bytes are copied.
        if (length === 0) {
            typedArray.slice(0);
        } else {
            assertThrowsInstanceOf(() => {
                typedArray.slice(0);
            }, TypeError, "ArrayBuffer is detached in TypedArraySpeciesCreate(...)");
        }
        assert.sameValue(detached, true, "$262.detachArrayBuffer was called");
    }
}

