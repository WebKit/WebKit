// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: A number as calendar in a property bag is not accepted
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    TypeError,
    () => instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" }),
    "Numbers cannot be used as a calendar"
  );
}
