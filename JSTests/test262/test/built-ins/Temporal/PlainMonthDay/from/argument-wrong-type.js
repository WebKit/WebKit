// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or property bag for PlainMonthDay
features: [BigInt, Symbol, Temporal]
---*/

const rangeErrorTests = [
  [undefined, "undefined"],
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
];

for (const [arg, description] of rangeErrorTests) {
  assert.throws(RangeError, () => Temporal.PlainMonthDay.from(arg), `${description} does not convert to a valid ISO string`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],
  [Temporal.PlainMonthDay, "Temporal.PlainMonthDay, object"],
  [Temporal.PlainMonthDay.prototype, "Temporal.PlainMonthDay.prototype, object"],
];

for (const [arg, description] of typeErrorTests) {
  assert.throws(TypeError, () => Temporal.PlainMonthDay.from(arg), `${description} is not a valid property bag and does not convert to a string`);
}
