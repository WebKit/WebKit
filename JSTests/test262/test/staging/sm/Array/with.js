// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/

Object.defineProperty(Array.prototype, 0, {
  set() {
    throw "bad";
  },
});

// Single element case.
assert.deepEqual([0].with(0, 1), [1]);

// More than one element.
assert.deepEqual([1, 2].with(0, 3), [3, 2]);
assert.deepEqual([1, 2].with(1, 3), [1, 3]);

