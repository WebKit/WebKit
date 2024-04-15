// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Temporal.Calendar.prototype.monthDayFromFields will return correctly with data and overflow set to 'constrain'.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Set fields to ? PrepareTemporalFields(fields, « "day", "month", "monthCode", "year" », « "day" »).
  7. Let overflow be ? ToTemporalOverflow(options).
  8. Perform ? ISOResolveMonth(fields).
  9. Let result be ? ISOMonthDayFromFields(fields, overflow).
  10. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]], "iso8601", result.[[ReferenceISOYear]]).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");
const opt = { overflow: "constrain" };

let result = cal.monthDayFromFields({ year: 2021, month: 13, day: 1 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 1, "month 13 is constrained to 12");

result = cal.monthDayFromFields({ year: 2021, month: 999999, day: 500 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "month 999999 is constrained to 12 and day 500 is constrained to 31");

[-99999, -1, 0].forEach((month) => {
  assert.throws(
    RangeError,
    () => cal.monthDayFromFields({ year: 2021, month, day: 1 }, opt),
    `Month ${month} is out of range for 2021 even with overflow: constrain`
  );
});

TemporalHelpers.ISOMonths.forEach(({ month, monthCode, daysInMonth }) => {
  const day = daysInMonth + 1;

  result = cal.monthDayFromFields({ month, day }, opt);
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth,
    `day is constrained from ${day} to ${daysInMonth} in month ${month}`);

  result = cal.monthDayFromFields({ month, day: 9001 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth,
    `day is constrained to ${daysInMonth} in month ${month}`);

  result = cal.monthDayFromFields({ monthCode, day }, opt);
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth,
    `day is constrained from ${day} to ${daysInMonth} in monthCode ${monthCode}`);

  result = cal.monthDayFromFields({ monthCode, day: 9001 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth,
    `day is constrained to ${daysInMonth} in monthCode ${monthCode}`);
});

[ ["month", 2], ["monthCode", "M02"] ].forEach(([ name, value ]) => {
  result = cal.monthDayFromFields({ year: 2020, [name]: value, day: 30 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M02", 29, `${name} ${value} is constrained to 29 in leap year 2020`);

  result = cal.monthDayFromFields({ year: 2021, [name]: value, day: 29 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M02", 28, `${name} ${value} is constrained to 28 in common year 2021`);
});
