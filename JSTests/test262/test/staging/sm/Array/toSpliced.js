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
    throw "bad 0";
  },
});

Object.defineProperty(Array.prototype, 1, {
  set() {
    throw "bad 1";
  },
});

assert.deepEqual([].toSpliced(0, 0, 1), [1]);

assert.deepEqual([0].toSpliced(0, 0, 0), [0, 0]);
assert.deepEqual([0].toSpliced(0, 0, 1), [1, 0]);
assert.deepEqual([0].toSpliced(0, 1, 0), [0]);
assert.deepEqual([0].toSpliced(0, 1, 1), [1]);
assert.deepEqual([0].toSpliced(1, 0, 0), [0, 0]);
assert.deepEqual([0].toSpliced(1, 0, 1), [0, 1]);
assert.deepEqual([0].toSpliced(1, 1, 0), [0, 0]);
assert.deepEqual([0].toSpliced(1, 1, 1), [0, 1]);

