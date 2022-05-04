// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: >
    Calendar.yearMonthFromFields method is called with undefined as the options
    value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
Temporal.PlainYearMonth.compare({ year: 2000, month: 5, calendar }, { year: 2000, month: 6, calendar });
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 2);

// Test again, but overriding the global Temporal.Calendar.prototype method so
// we can observe the call to yearMonthFromFields() on the ISO8601 calendar
// that occurs when we parse the string

const realYearMonthFromFields = Temporal.Calendar.prototype.yearMonthFromFields;
let yearMonthFromFieldsCallCount = 0;
Temporal.Calendar.prototype.yearMonthFromFields = function (fields, options) {
  yearMonthFromFieldsCallCount++;
  assert.sameValue(options, undefined, "yearMonthFromFields shouldn't be called with options");
  return realYearMonthFromFields.call(this, fields, options);
}

Temporal.PlainYearMonth.compare("2000-05-01", "2000-06-01");
assert.sameValue(yearMonthFromFieldsCallCount, 2);

Temporal.Calendar.prototype.yearMonthFromFields = realYearMonthFromFields;
