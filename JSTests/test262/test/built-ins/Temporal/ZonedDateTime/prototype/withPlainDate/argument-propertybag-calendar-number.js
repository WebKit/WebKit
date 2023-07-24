// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);

const calendar = 19970327;

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result = instance.withPlainDate(arg);
assert.sameValue(result.epochNanoseconds, 217_129_600_000_000_000n, "19970327 is a valid ISO string for calendar");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => instance.withPlainDate(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
}
