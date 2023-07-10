// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const calendar = 19970327;

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result = instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" });
assert.sameValue(result.epochNanoseconds, 217_168_496_987_654_321n, "19970327 is a valid ISO string for calendar");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" }),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
}
