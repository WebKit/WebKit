// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or property bag for PlainTime
features: [BigInt, Symbol, Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
];

for (const [arg, description] of rangeErrorTests) {
  assert.throws(RangeError, () => instance.toZonedDateTime({ plainTime: arg, timeZone: "UTC" }), `${description} does not convert to a valid ISO string`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],
  [Temporal.PlainTime, "Temporal.PlainTime, object"],
  [Temporal.PlainTime.prototype, "Temporal.PlainTime.prototype, object"],
];

for (const [arg, description] of typeErrorTests) {
  assert.throws(TypeError, () => instance.toZonedDateTime({ plainTime: arg, timeZone: "UTC" }), `${description} is not a valid property bag and does not convert to a string`);
}
