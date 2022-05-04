// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.equals
description: >
    Calendar.yearMonthFromFields method is called with undefined as the options
    value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
let instance = new Temporal.PlainYearMonth(2000, 5, calendar);
instance.equals({ year: 2000, month: 6, calendar });
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1);

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

calendar = new Temporal.Calendar("iso8601");
instance = new Temporal.PlainYearMonth(2000, 5, calendar);
instance.equals("2000-06-01");
assert.sameValue(yearMonthFromFieldsCallCount, 1);

Temporal.Calendar.prototype.yearMonthFromFields = realYearMonthFromFields;
