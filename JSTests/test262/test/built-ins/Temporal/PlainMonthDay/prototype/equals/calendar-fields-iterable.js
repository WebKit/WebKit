// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainmonthday.prototype.equals step 3:
      3. Set _other_ to ? ToTemporalMonthDay(_other_).
    sec-temporal-totemporalmonthday step 2.f:
      f. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"month"*, *"monthCode"*, *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
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
const date = new Temporal.PlainMonthDay(5, 2, calendar1);
const calendar2 = TemporalHelpers.calendarFieldsIterable();
date.equals({ monthCode: "M06", day: 2, calendar: calendar2 });

assert.sameValue(calendar1.fieldsCallCount, 0, "fields() method not called");
assert.sameValue(calendar2.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar2.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar2.iteratorExhausted[0], "iterated through the whole iterable");
