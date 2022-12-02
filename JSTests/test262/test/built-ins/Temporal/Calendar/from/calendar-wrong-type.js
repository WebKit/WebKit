// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or object for Calendar
features: [BigInt, Symbol, Temporal]
---*/

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
  [new Temporal.TimeZone("UTC"), "time zone instance"],
];

for (const [arg, description] of rangeErrorTests) {
  assert.throws(RangeError, () => Temporal.Calendar.from(arg), `${description} does not convert to a valid ISO string`);
  assert.throws(RangeError, () => Temporal.Calendar.from({ calendar: arg }), `${description} does not convert to a valid ISO string (in property bag)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
];

for (const [arg, description] of typeErrorTests) {
  assert.throws(TypeError, () => Temporal.Calendar.from(arg), `${description} is not a valid object and does not convert to a string`);
  assert.throws(TypeError, () => Temporal.Calendar.from({ calendar: arg }), `${description} is not a valid object and does not convert to a string (in property bag)`);
}
