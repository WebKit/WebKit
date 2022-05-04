// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Temporal.Calendar.prototype.yearMonthFromFields return correctly with valid data.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISOYearMonthFromFields(fields, options).
  7. Return ? CreateTemporalYearMonth(result.[[Year]], result.[[Month]], calendar, result.[[ReferenceISODay]]).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601")

let result = cal.yearMonthFromFields({ year: 2021, month: 7 });
TemporalHelpers.assertPlainYearMonth(result, 2021, 7, "M07", "year 2021, month 7");
result = cal.yearMonthFromFields({ year: 2021, month: 12 });
TemporalHelpers.assertPlainYearMonth(result, 2021, 12, "M12", "year 2021, month 12");
result = cal.yearMonthFromFields({ year: 2021, monthCode: "M07" });
TemporalHelpers.assertPlainYearMonth(result, 2021, 7, "M07", "year 2021, monthCode M07");
result = cal.yearMonthFromFields({ year: 2021, monthCode: "M12" });
TemporalHelpers.assertPlainYearMonth(result, 2021, 12, "M12", "year 2021, monthCode M12");

["constrain", "reject"].forEach((overflow) => {
  const opt = { overflow };
  result = cal.yearMonthFromFields({ year: 2021, month: 7 }, opt);
  TemporalHelpers.assertPlainYearMonth(result, 2021, 7, "M07", `year 2021, month 7, overflow ${overflow}`);
  result = cal.yearMonthFromFields({ year: 2021, month: 12 }, opt);
  TemporalHelpers.assertPlainYearMonth(result, 2021, 12, "M12", `year 2021, month 12, overflow ${overflow}`);
  result = cal.yearMonthFromFields({ year: 2021, monthCode: "M07" }, opt);
  TemporalHelpers.assertPlainYearMonth(result, 2021, 7, "M07", `year 2021, monthCode M07, overflow ${overflow}`);
  result = cal.yearMonthFromFields({ year: 2021, monthCode: "M12" }, opt);
  TemporalHelpers.assertPlainYearMonth(result, 2021, 12, "M12", `year 2021, monthCode M12, overflow ${overflow}`);
});
