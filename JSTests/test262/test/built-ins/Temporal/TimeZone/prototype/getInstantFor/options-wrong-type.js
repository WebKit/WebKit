// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: TypeError thrown when options argument is a primitive
features: [BigInt, Symbol, Temporal]
---*/

const badOptions = [
  null,
  true,
  "some string",
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.TimeZone("UTC");
for (const value of badOptions) {
  assert.throws(TypeError, () => instance.getInstantFor(new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38), value),
    `TypeError on wrong options type ${typeof value}`);
};
