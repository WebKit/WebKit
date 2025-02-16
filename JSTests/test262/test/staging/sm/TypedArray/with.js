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

class Err {}

const indices = [
  -Infinity, -10, -0.5, -0, 0, 0.5, 10, Infinity, NaN
];

let value = {
  valueOf() {
    throw new Err;
  }
};

let ta = new Int32Array(5);
for (let index of indices) {
  assertThrowsInstanceOf(() => ta.with(index, value), Err);
}

for (let index of indices) {
  let ta = new Int32Array(5);

  let value = {
    valueOf() {
      $262.detachArrayBuffer(ta.buffer);
      return 0;
    }
  };

  assertThrowsInstanceOf(() => ta.with(index, value), RangeError);
}

