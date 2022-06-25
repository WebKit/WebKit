// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainyearmonth.prototype.since step 3:
      3. Set _other_ to ? ToTemporalYearMonth(_other_).
    sec-temporal.plainyearmonth.prototype.since step 14:
      14. Let fieldNames be ? CalendarFields(_calendar_, « *"monthCode"*, *"year"* »).
    sec-temporal-totemporalyearmonth step 2.c:
      c. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"month"*, *"monthCode"*, *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected1 = [
  "monthCode",
  "year",
];
const expected2 = [
  "month",
  "monthCode",
  "year",
];

const calendar1 = TemporalHelpers.calendarFieldsIterable();
const yearmonth = new Temporal.PlainYearMonth(2000, 5, calendar1);
const calendar2 = TemporalHelpers.calendarFieldsIterable();
yearmonth.since({ year: 2005, month: 6, calendar: calendar2 });

assert.sameValue(calendar1.fieldsCallCount, 1, "fields() method not called");
assert.compareArray(calendar1.fieldsCalledWith[0], expected1, "fields() method called with correct args");
assert(calendar1.iteratorExhausted[0], "iterated through the whole iterable");
assert.sameValue(calendar2.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar2.fieldsCalledWith[0], expected2, "fields() method called with correct args");
assert(calendar2.iteratorExhausted[0], "iterated through the whole iterable");
