// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: A number as calendar in a property bag is converted to a string, then to a calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = 19970327;

let arg = { monthCode: "M11", day: 18, calendar };
const result1 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(result1, "M11", 18, "19970327 is a valid ISO string for calendar");

arg = { monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(result2, "M11", 18, "19970327 is a valid ISO string for calendar (nested property)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let arg = { monthCode: "M11", day: 18, calendar };
  assert.throws(
    RangeError,
    () => Temporal.PlainMonthDay.from(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
  arg = { monthCode: "M11", day: 18, calendar: { calendar } };
  assert.throws(
    RangeError,
    () => Temporal.PlainMonthDay.from(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property)`
  );
}
