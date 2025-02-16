// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  `take` and `drop` throw eagerly when passed negative numbers, after rounding towards 0.
info: |
  Iterator Helpers proposal 2.1.5.4 and 2.1.5.5
features:
  - iterator-helpers
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
---*/

//
//
const iter = [].values();
const methods = [
  value => iter.take(value),
  value => iter.drop(value),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(-1), RangeError);
  assertThrowsInstanceOf(() => method(-Infinity), RangeError);
  assertThrowsInstanceOf(() => method(NaN), RangeError);
  assertThrowsInstanceOf(() => method(-NaN), RangeError);

  method(-0);
  method(-0.9);
}

