// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Temporal.Calendar.prototype.monthDayFromFields will return correctly with valid data.
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

const options = [
  { overflow: "constrain" },
  { overflow: "reject" },
  {},
  undefined,
];
options.forEach((opt) => {
  const optionsDesc = opt && JSON.stringify(opt);
  let result = cal.monthDayFromFields({ year: 2021, month: 7, day: 3 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M07", 3, `month 7, day 3, with year, options = ${optionsDesc}`);
  result = cal.monthDayFromFields({ year: 2021, month: 12, day: 31 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M12", 31, `month 12, day 31, with year, options = ${optionsDesc}`);
  result = cal.monthDayFromFields({ monthCode: "M07", day: 3 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M07", 3, `monthCode M07, day 3, options = ${optionsDesc}`);
  result = cal.monthDayFromFields({ monthCode: "M12", day: 31 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M12", 31, `monthCode M12, day 31, options = ${optionsDesc}`);
});

TemporalHelpers.ISOMonths.forEach(({ month, monthCode, daysInMonth }) => {
  let result = cal.monthDayFromFields({ month, day: daysInMonth });
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth, `month ${month}, day ${daysInMonth}`);

  result = cal.monthDayFromFields({ monthCode, day: daysInMonth });
  TemporalHelpers.assertPlainMonthDay(result, monthCode, daysInMonth, `monthCode ${monthCode}, day ${daysInMonth}`);
});
