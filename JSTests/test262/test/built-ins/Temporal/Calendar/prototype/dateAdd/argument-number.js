// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: A number is converted to a string, then to Temporal.PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const arg = 19761118;

const result = instance.dateAdd(arg, new Temporal.Duration());
TemporalHelpers.assertPlainDate(result, 1976, 11, "M11", 18, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.dateAdd(arg, new Temporal.Duration()),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}
