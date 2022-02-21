// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainyearmonth.prototype.with step 9:
      9. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"month"*, *"monthCode"*, *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "month",
  "monthCode",
  "year",
];

const calendar = TemporalHelpers.calendarFieldsIterable();
const yearmonth = new Temporal.PlainYearMonth(2000, 5, calendar);
yearmonth.with({ year: 2005 });

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
