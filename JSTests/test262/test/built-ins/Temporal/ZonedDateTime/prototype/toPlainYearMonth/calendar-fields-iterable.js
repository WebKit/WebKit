// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainyearmonth
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.zoneddatetime.prototype.toplainyearmonth step 7:
      7. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"monthCode"*, *"year"* »).
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
const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
datetime.toPlainYearMonth();

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
