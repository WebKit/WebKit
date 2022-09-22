// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: >
  Appropriate error thrown when argument cannot be converted to a valid string
  for Instant
features: [BigInt, Symbol, Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const rangeErrorTests = [
  [undefined, "undefined"],
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [19761118, "number that would convert to a valid ISO string in other contexts"],
  [1n, "bigint"],
  [{}, "plain object"],
  [Temporal.Instant, "Temporal.Instant, object"],
];

for (const [arg, description] of rangeErrorTests) {
  assert.throws(RangeError, () => instance.getPreviousTransition(arg), `${description} does not convert to a valid ISO string`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [Temporal.Instant.prototype, "Temporal.Instant.prototype, object"],  // fails brand check in toString()
];

for (const [arg, description] of typeErrorTests) {
  assert.throws(TypeError, () => instance.getPreviousTransition(arg), `${description} does not convert to a string`);
}
