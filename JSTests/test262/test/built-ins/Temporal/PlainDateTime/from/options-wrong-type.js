// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
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

for (const value of badOptions) {
  assert.throws(TypeError, () => Temporal.PlainDateTime.from({ year: 1976, month: 11, day: 18 }, value),
    `TypeError on wrong options type ${typeof value}`);
};
