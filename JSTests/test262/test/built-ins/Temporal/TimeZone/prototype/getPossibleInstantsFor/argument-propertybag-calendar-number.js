// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: A number as calendar in a property bag is converted to a string, then to a calendar
includes: [compareArray.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const calendar = 19970327;

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result = instance.getPossibleInstantsFor(arg);
assert.compareArray(result.map(i => i.epochNanoseconds), [217_123_200_000_000_000n], "19970327 is a valid ISO string for calendar");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => instance.getPossibleInstantsFor(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
}
