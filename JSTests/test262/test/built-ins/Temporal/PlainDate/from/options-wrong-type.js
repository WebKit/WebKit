// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
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
  assert.throws(TypeError, () => Temporal.PlainDate.from({ year: 1976, month: 11, day: 18 }, value),
    `TypeError on wrong options type ${typeof value}`);
  assert.throws(TypeError, () => Temporal.PlainDate.from(new Temporal.PlainDate(1976, 11, 18), value),
    "TypeError thrown before cloning PlainDate instance");
  assert.throws(TypeError, () => Temporal.PlainDate.from(new Temporal.ZonedDateTime(0n, "UTC"), value),
    "TypeError thrown before converting ZonedDateTime instance");
  assert.throws(TypeError, () => Temporal.PlainDate.from(new Temporal.PlainDateTime(1976, 11, 18), value),
    "TypeError thrown before converting PlainDateTime instance");
  assert.throws(RangeError, () => Temporal.PlainDate.from("1976-11-18Z", value),
    "Invalid string processed before throwing TypeError");
};
