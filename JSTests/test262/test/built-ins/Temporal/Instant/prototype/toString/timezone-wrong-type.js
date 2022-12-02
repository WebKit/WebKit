// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  or object for TimeZone
features: [BigInt, Symbol, Temporal]
---*/

const instance = new Temporal.Instant(0n);

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [19761118, "number that would convert to a valid ISO string in other contexts"],
  [1n, "bigint"],
  [new Temporal.Calendar("iso8601"), "calendar instance"],
];

for (const [timeZone, description] of rangeErrorTests) {
  assert.throws(RangeError, () => instance.toString({ timeZone }), `${description} does not convert to a valid ISO string`);
  assert.throws(RangeError, () => instance.toString({ timeZone: { timeZone } }), `${description} does not convert to a valid ISO string (nested property)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
];

for (const [timeZone, description] of typeErrorTests) {
  assert.throws(TypeError, () => instance.toString({ timeZone }), `${description} is not a valid object and does not convert to a string`);
  assert.throws(TypeError, () => instance.toString({ timeZone: { timeZone } }), `${description} is not a valid object and does not convert to a string (nested property)`);
}

const timeZone = undefined;
assert.throws(RangeError, () => instance.toString({ timeZone: { timeZone } }), `undefined is always a RangeError as nested property`);
