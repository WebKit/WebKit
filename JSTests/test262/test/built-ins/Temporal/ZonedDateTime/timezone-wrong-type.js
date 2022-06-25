// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or object for TimeZone
features: [BigInt, Symbol, Temporal]
---*/

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [19761118, "number that would convert to a valid ISO string in other contexts"],
  [1n, "bigint"],
];

for (const [timeZone, description] of rangeErrorTests) {
  assert.throws(RangeError, () => new Temporal.ZonedDateTime(0n, timeZone), `${description} does not convert to a valid ISO string`);
  assert.throws(RangeError, () => new Temporal.ZonedDateTime(0n, { timeZone }), `${description} does not convert to a valid ISO string (nested property)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
];

for (const [timeZone, description] of typeErrorTests) {
  assert.throws(TypeError, () => new Temporal.ZonedDateTime(0n, timeZone), `${description} is not a valid object and does not convert to a string`);
  assert.throws(TypeError, () => new Temporal.ZonedDateTime(0n, { timeZone }), `${description} is not a valid object and does not convert to a string (nested property)`);
}

const timeZone = undefined;
assert.throws(RangeError, () => new Temporal.ZonedDateTime(0n, { timeZone }), `undefined is always a RangeError as nested property`);
