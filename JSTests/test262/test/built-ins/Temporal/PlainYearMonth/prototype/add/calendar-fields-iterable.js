// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainyearmonth.prototype.add step 8:
      8. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"monthCode"*, *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "monthCode",
  "year",
];

const calendar = TemporalHelpers.calendarFieldsIterable();
const yearmonth = new Temporal.PlainYearMonth(2000, 5, calendar);
yearmonth.add({ months: 1 });

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
