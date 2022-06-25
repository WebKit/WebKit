// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: A number as calendar in a property bag is converted to a string, then to a calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const calendar = 19970327;

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.dateUntil(arg, new Temporal.PlainDate(1976, 11, 19));
TemporalHelpers.assertDuration(result1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "19970327 is a valid ISO string for calendar (first argument)");
const result2 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 19), arg);
TemporalHelpers.assertDuration(result2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "19970327 is a valid ISO string for calendar (second argument)");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result3 = instance.dateUntil(arg, new Temporal.PlainDate(1976, 11, 19));
TemporalHelpers.assertDuration(result3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "19970327 is a valid ISO string for calendar (nested property, first argument)");
const result4 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 19), arg);
TemporalHelpers.assertDuration(result4, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "19970327 is a valid ISO string for calendar (nested property, second argument)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 19)),
    `Number ${calendar} does not convert to a valid ISO string for calendar (first argument)`
  );
  assert.throws(
    RangeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 19), arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (second argument)`
  );
  arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
  assert.throws(
    RangeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 19)),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property, first argument)`
  );
  assert.throws(
    RangeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 19), arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property, second argument)`
  );
}
