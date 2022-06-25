// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or property bag for ZonedDateTime
features: [BigInt, Symbol, Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const rangeErrorTests = [
  [undefined, "undefined"],
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [19761118, "number that would convert to a valid ISO string in other contexts"],
  [1n, "bigint"],
];

for (const [arg, description] of rangeErrorTests) {
  assert.throws(RangeError, () => instance.equals(arg), `${description} does not convert to a valid ISO string`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],
  [Temporal.ZonedDateTime, "Temporal.ZonedDateTime, object"],
  [Temporal.ZonedDateTime.prototype, "Temporal.ZonedDateTime.prototype, object"],
];

for (const [arg, description] of typeErrorTests) {
  assert.throws(TypeError, () => instance.equals(arg), `${description} is not a valid property bag and does not convert to a string`);
}
