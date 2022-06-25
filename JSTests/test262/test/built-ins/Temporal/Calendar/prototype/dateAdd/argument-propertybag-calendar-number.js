// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: A number as calendar in a property bag is converted to a string, then to a calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const calendar = 19970327;

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.dateAdd(arg, new Temporal.Duration());
TemporalHelpers.assertPlainDate(result1, 1976, 11, "M11", 18, "19970327 is a valid ISO string for calendar");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = instance.dateAdd(arg, new Temporal.Duration());
TemporalHelpers.assertPlainDate(result2, 1976, 11, "M11", 18, "19970327 is a valid ISO string for calendar (nested property)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => instance.dateAdd(arg, new Temporal.Duration()),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
  arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
  assert.throws(
    RangeError,
    () => instance.dateAdd(arg, new Temporal.Duration()),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property)`
  );
}
