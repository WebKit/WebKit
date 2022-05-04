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
  6. Let result be ? ISOMonthDayFromFields(fields, options).
  7. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]], calendar, result.[[ReferenceISOYear]]).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");
const opt = { overflow: "constrain" };

let result = cal.monthDayFromFields({ year: 2021, month: 1, day: 133 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M01", 31, "day is constrained to 31 in month 1");
result = cal.monthDayFromFields({ year: 2021, month: 2, day: 133 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M02", 28, "day is constrained to 28 in month 2 (year 2021)");
result = cal.monthDayFromFields({ year: 2021, month: 3, day: 9033 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M03", 31, "day is constrained to 31 in month 3");
result = cal.monthDayFromFields({ year: 2021, month: 4, day: 50 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M04", 30, "day is constrained to 30 in month 4");
result = cal.monthDayFromFields({ year: 2021, month: 5, day: 77 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M05", 31, "day is constrained to 31 in month 5");
result = cal.monthDayFromFields({ year: 2021, month: 6, day: 33 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M06", 30, "day is constrained to 30 in month 6");
result = cal.monthDayFromFields({ year: 2021, month: 7, day: 33 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M07", 31, "day is constrained to 31 in month 7");
result = cal.monthDayFromFields({ year: 2021, month: 8, day: 300 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M08", 31, "day is constrained to 31 in month 8");
result = cal.monthDayFromFields({ year: 2021, month: 9, day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M09", 30, "day is constrained to 30 in month 9");
result = cal.monthDayFromFields({ year: 2021, month: 10, day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M10", 31, "day is constrained to 31 in month 10");
result = cal.monthDayFromFields({ year: 2021, month: 11, day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M11", 30, "day is constrained to 30 in month 11");
result = cal.monthDayFromFields({ year: 2021, month: 12, day: 500 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "day is constrained to 31 in month 12");

assert.throws(
  RangeError,
  () => cal.monthDayFromFields({ year: 2021, month: -99999, day: 1 }, opt),
  "negative month -99999 is out of range even with overflow constrain"
)
assert.throws(
  RangeError,
  () => cal.monthDayFromFields({ year: 2021, month: -1, day: 1 }, opt),
  "negative month -1 is out of range even with overflow constrain"
)
assert.throws(
  RangeError,
  () => cal.monthDayFromFields({ year: 2021, month: 0, day: 1 }, opt),
  "month zero is out of range even with overflow constrain"
)

result = cal.monthDayFromFields({ year: 2021, month: 13, day: 1 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 1, "month 13 is constrained to 12");
result = cal.monthDayFromFields({ year: 2021, month: 999999, day: 500 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "month 999999 is constrained to 12 and day constrained to 31");

result = cal.monthDayFromFields({ monthCode: "M01", day: 133 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M01", 31, "day is constrained to 31 in monthCode M01");
result = cal.monthDayFromFields({ monthCode: "M02", day: 133 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M02", 29, "day is constrained to 29 in monthCode M02");
result = cal.monthDayFromFields({ monthCode: "M03", day: 9033 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M03", 31, "day is constrained to 31 in monthCode M03");
result = cal.monthDayFromFields({ monthCode: "M04", day: 50 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M04", 30, "day is constrained to 30 in monthCode M04");
result = cal.monthDayFromFields({ monthCode: "M05", day: 77 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M05", 31, "day is constrained to 31 in monthCode M05");
result = cal.monthDayFromFields({ monthCode: "M06", day: 33 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M06", 30, "day is constrained to 30 in monthCode M06");
result = cal.monthDayFromFields({ monthCode: "M07", day: 33 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M07", 31, "day is constrained to 31 in monthCode M07");
result = cal.monthDayFromFields({ monthCode: "M08", day: 300 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M08", 31, "day is constrained to 31 in monthCode M08");
result = cal.monthDayFromFields({ monthCode: "M09", day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M09", 30, "day is constrained to 30 in monthCode M09");
result = cal.monthDayFromFields({ monthCode: "M10", day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M10", 31, "day is constrained to 31 in monthCode M10");
result = cal.monthDayFromFields({ monthCode: "M11", day: 400 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M11", 30, "day is constrained to 30 in monthCode M11");
result = cal.monthDayFromFields({ monthCode: "M12", day: 500 }, opt);
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "day is constrained to 31 in monthCode M12");
