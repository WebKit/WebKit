// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: TypeError thrown when options argument is missing or a non-string primitive
features: [BigInt, Symbol, Temporal]
---*/

const badOptions = [
  undefined,
  null,
  true,
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.Duration(0, 0, 0, 0, 1);
assert.throws(TypeError, () => instance.total(), "TypeError on missing options argument");
for (const value of badOptions) {
  assert.throws(TypeError, () => instance.total(value),
    `TypeError on wrong options type ${typeof value}`);
};
