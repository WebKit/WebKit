// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.with
description: The object returned from mergeFields() is copied before being passed to monthDayFromFields().
info: |
    sec-temporal.plainmonthday.prototype.with steps 13–15:
      13. Set _fields_ to ? CalendarMergeFields(_calendar_, _fields_, _partialMonthDay_).
      14. Set _fields_ to ? PrepareTemporalFields(_fields_, _fieldNames_, «»).
      15. Return ? MonthDayFromFields(_calendar_, _fields_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get day",
  "get day.valueOf",
  "call day.valueOf",
  "get month", // PlainMonthDay.month property does not exist, no valueOf
  "get monthCode",
  "get monthCode.toString",
  "call monthCode.toString",
  "get year", // undefined, no valueOf
];

const calendar = TemporalHelpers.calendarMergeFieldsGetters();
const monthday = new Temporal.PlainMonthDay(3, 31, calendar);
monthday.with({ day: 1 });

assert.compareArray(calendar.mergeFieldsReturnOperations, expected, "getters called on mergeFields return");
