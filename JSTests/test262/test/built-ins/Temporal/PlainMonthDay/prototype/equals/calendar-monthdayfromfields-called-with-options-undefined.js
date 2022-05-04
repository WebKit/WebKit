// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: >
    Calendar.monthDayFromFields method is called with undefined as the options
    value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
const instance = new Temporal.PlainMonthDay(5, 2, calendar);
instance.equals({ monthCode: "M05", day: 3, calendar });
assert.sameValue(calendar.monthDayFromFieldsCallCount, 1);

// Test again, but overriding the global Temporal.Calendar.prototype method so
// we can observe the call to monthDayFromFields() on the ISO8601 calendar
// that occurs when we parse the string

const realMonthDayFromFields = Temporal.Calendar.prototype.monthDayFromFields;
let monthDayFromFieldsCallCount = 0;
Temporal.Calendar.prototype.monthDayFromFields = function (fields, options) {
  monthDayFromFieldsCallCount++;
  assert.sameValue(options, undefined, "monthDayFromFields shouldn't be called with options");
  return realMonthDayFromFields.call(this, fields, options);
}

instance.equals("2000-05-03");
assert.sameValue(monthDayFromFieldsCallCount, 1);

Temporal.Calendar.prototype.monthDayFromFields = realMonthDayFromFields;
