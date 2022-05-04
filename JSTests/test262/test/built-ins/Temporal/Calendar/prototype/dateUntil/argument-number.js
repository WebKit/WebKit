// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: A number is converted to a string, then to Temporal.PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const arg = 19761118;

const result1 = instance.dateUntil(arg, new Temporal.PlainDate(1976, 11, 19));
TemporalHelpers.assertDuration(result1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "19761118 is a valid ISO string for PlainDate (first argument)");
const result2 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 19), arg);
TemporalHelpers.assertDuration(result2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "19761118 is a valid ISO string for PlainDate (second argument)");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 18)),
    `Number ${arg} does not convert to a valid ISO string for PlainDate (first argument)`
  );
  assert.throws(
    RangeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 18), arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate (second argument)`
  );
}
