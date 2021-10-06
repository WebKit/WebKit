// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: TypeError thrown when options argument is missing or a primitive
features: [BigInt, Symbol, Temporal]
---*/

const values = [
  undefined,
  null,
  true,
  "string",
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.Duration(0, 0, 0, 0, 1);
assert.throws(TypeError, () => instance.total(), "TypeError on missing argument");
values.forEach((value) => {
  assert.throws(TypeError, () => instance.total(value), `TypeError on wrong argument type ${typeof(value)}`);
});
