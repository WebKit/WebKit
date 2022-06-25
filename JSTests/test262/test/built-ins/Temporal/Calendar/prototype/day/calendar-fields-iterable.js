// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.calendar.prototype.day step 4:
      4. Return ? ISODay(_dateOrDateTime_).
    sec-temporal-isoday step 1.a:
      a. Set _dateOrDateTime_ to ? ToTemporalDate(_dateOrDateTime_).
    sec-temporal-totemporaldate step 2.c:
      c. Let _fieldNames_ be ? CalendarFields(calendar, « *"day"*, *"month"*, *"monthCode"*, *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToListOfType(_fieldsArray_, « String »).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "day",
  "month",
  "monthCode",
  "year",
];

const calendar1 = TemporalHelpers.calendarFieldsIterable();
const calendar2 = TemporalHelpers.calendarFieldsIterable();
calendar1.day({ year: 2000, month: 5, day: 2, calendar: calendar2 });

assert.sameValue(calendar1.fieldsCallCount, 0, "fields() method not called");
assert.sameValue(calendar2.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar2.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar2.iteratorExhausted[0], "iterated through the whole iterable");
