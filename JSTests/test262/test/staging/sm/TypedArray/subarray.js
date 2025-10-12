// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-TypedArray-shell.js]
description: |
  pending
esid: pending
---*/

for (let constructor of typedArrayConstructors) {
    const elementSize = constructor.BYTES_PER_ELEMENT;

    let targetOffset;
    let buffer = new ArrayBuffer(2 * elementSize);
    let typedArray = new constructor(buffer, 1 * elementSize, 1);
    typedArray.constructor = {
        [Symbol.species]: function(ab, offset, length) {
            targetOffset = offset;
            return new constructor(1);
        }
    };

    let beginIndex = {
        valueOf() {
            $262.detachArrayBuffer(buffer);
            return 0;
        }
    };
    typedArray.subarray(beginIndex);

    assert.sameValue(targetOffset, 1 * elementSize);
}
