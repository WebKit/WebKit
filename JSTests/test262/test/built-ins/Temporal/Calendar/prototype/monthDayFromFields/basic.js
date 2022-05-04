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
  6. Let result be ? ISOMonthDayFromFields(fields, options).
  7. Return ? CreateTemporalMonthDay(result.[[Month]], result.[[Day]], calendar, result.[[ReferenceISOYear]]).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

let result = cal.monthDayFromFields({ year: 2021, month: 7, day: 3 });
TemporalHelpers.assertPlainMonthDay(result, "M07", 3, "month 7, day 3, with year");
result = cal.monthDayFromFields({ year: 2021, month: 12, day: 31 });
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "month 12, day 31, with year");
result = cal.monthDayFromFields({ monthCode: "M07", day: 3 });
TemporalHelpers.assertPlainMonthDay(result, "M07", 3, "monthCode M07, day 3");
result = cal.monthDayFromFields({ monthCode: "M12", day: 31 });
TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "monthCode M12, day 31");

["constrain", "reject"].forEach(function (overflow) {
  const opt = { overflow };
  result = cal.monthDayFromFields({ year: 2021, month: 7, day: 3 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M07", 3, "month 7, day 3, with year");
  result = cal.monthDayFromFields({ year: 2021, month: 12, day: 31 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "month 12, day 31, with year");
  result = cal.monthDayFromFields({ monthCode: "M07", day: 3 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M07", 3, "monthCode M07, day 3");
  result = cal.monthDayFromFields({ monthCode: "M12", day: 31 }, opt);
  TemporalHelpers.assertPlainMonthDay(result, "M12", 31, "monthCode M12, day 31");
});
