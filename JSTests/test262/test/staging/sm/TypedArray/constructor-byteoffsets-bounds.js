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
// 22.2.4.5 TypedArray ( buffer [ , byteOffset [ , length ] ] )

// Test bound checks around for |byteOffset| and |length| arguments.

const ab = new ArrayBuffer(0);

for (let TA of typedArrayConstructors) {
    // Test bound checks around INT32_MAX for |byteOffset| argument.
    assertThrowsInstanceOf(() => new TA(ab, 2**31 - TA.BYTES_PER_ELEMENT), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**31 - 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**31), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**31 + 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**31 + TA.BYTES_PER_ELEMENT), RangeError);

    // Test bound checks around UINT32_MAX for |byteOffset| argument.
    assertThrowsInstanceOf(() => new TA(ab, 2**32 - TA.BYTES_PER_ELEMENT), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**32 - 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**32), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**32 + 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 2**32 + TA.BYTES_PER_ELEMENT), RangeError);

    // Test bound checks around INT32_MAX for |length| argument.
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**31 - 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**31), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**31 + 1), RangeError);

    // Test bound checks around UINT32_MAX for |length| argument.
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**32 - 1), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**32), RangeError);
    assertThrowsInstanceOf(() => new TA(ab, 0, 2**32 + 1), RangeError);
}

