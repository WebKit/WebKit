// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
const testCases = {
    // Pre-sorted test data, it's important that these arrays remain in ascending order.
    [Int8Array.name]: [[-128, 127]],
    [Int16Array.name]: [[-32768, -999, 1942, 32767]],
    [Int32Array.name]: [[-2147483648, -320000, -244000, 2147483647]],
    [Uint8Array.name]: [[255]],
    [Uint16Array.name]: [[0, 65535, 65535]],
    [Uint32Array.name]: [[0, 987632, 4294967295]],
    [Uint8ClampedArray.name]: [[255]],

    // Test the behavior in the default comparator as described in 22.2.3.26.
    // The spec boils down to, -0s come before +0s, and NaNs always come last.
    // Float Arrays are used because all other types convert -0 and NaN to +0.
    [Float16Array.name]: [
        [-2147483647, -2147483646.99, -0, 0, 2147483646.99, NaN],
        [1/undefined, NaN, Number.NaN]
    ],
    [Float32Array.name]: [
        [-2147483647, -2147483646.99, -0, 0, 2147483646.99, NaN],
        [1/undefined, NaN, Number.NaN]
    ],
    [Float64Array.name]: [
        [-2147483646.99, -0, 0, 4147483646.99, NaN],
        [1/undefined, NaN, Number.NaN]
    ],
};

// Sort every possible permutation of an arrays
function sortAllPermutations(dataType, testData) {
    let reference = new dataType(testData);
    for (let permutation of Permutations(testData))
        assert.deepEqual((new dataType(permutation)).sort(), reference);
}

for (let constructor of sharedTypedArrayConstructors) {
    for (let data of testCases[constructor.name]) {
        sortAllPermutations(constructor, data);
    }
}

