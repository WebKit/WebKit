// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js, compareArray.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Test %TypedArray%.prototype.set(typedArray, offset) when called with wrapped
// typed array.

if (typeof createNewGlobal === "function") {
    var otherGlobal = createNewGlobal();

    function taintLengthProperty(obj) {
        Object.defineProperty(obj, "length", {
            get() {
                assert.sameValue(true, false);
            }
        });
    }

    for (var TA of anyTypedArrayConstructors) {
        var target = new TA(4);
        var source = new otherGlobal[TA.name]([10, 20]);

        // Ensure "length" getter accessor isn't called.
        taintLengthProperty(source);

        assert.compareArray(target, [0, 0, 0, 0]);
        target.set(source, 1);
        assert.compareArray(target, [0, 10, 20, 0]);
    }

    // Detachment checks are also applied correctly for wrapped typed arrays.
    if (typeof $262.detachArrayBuffer === "function") {
        // Create typed array from different global (explicit constructor call).
        for (var TA of typedArrayConstructors) {
            var target = new TA(4);
            var source = new otherGlobal[TA.name](1);
            taintLengthProperty(source);

            // Called with wrapped typed array, array buffer already detached.
            otherGlobal.$262.detachArrayBuffer(source.buffer);
            assertThrowsInstanceOf(() => target.set(source), TypeError);

            var source = new otherGlobal[TA.name](1);
            taintLengthProperty(source);

            // Called with wrapped typed array, array buffer detached when
            // processing offset parameter.
            var offset = {
                valueOf() {
                    otherGlobal.$262.detachArrayBuffer(source.buffer);
                    return 0;
                }
            };
            assertThrowsInstanceOf(() => target.set(source, offset), TypeError);
        }

        // Create typed array from different global (implictly created when
        // ArrayBuffer is a CCW).
        for (var TA of typedArrayConstructors) {
            var target = new TA(4);
            var source = new TA(new otherGlobal.ArrayBuffer(1 * TA.BYTES_PER_ELEMENT));
            taintLengthProperty(source);

            // Called with wrapped typed array, array buffer already detached.
            otherGlobal.$262.detachArrayBuffer(source.buffer);
            assertThrowsInstanceOf(() => target.set(source), TypeError);

            var source = new TA(new otherGlobal.ArrayBuffer(1 * TA.BYTES_PER_ELEMENT));
            taintLengthProperty(source);

            // Called with wrapped typed array, array buffer detached when
            // processing offset parameter.
            var offset = {
                valueOf() {
                    otherGlobal.$262.detachArrayBuffer(source.buffer);
                    return 0;
                }
            };
            assertThrowsInstanceOf(() => target.set(source, offset), TypeError);
        }
    }
}

