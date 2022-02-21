// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainyearmonth.from step 3:
      3. Return ? ToTemporalYearMonth(_item_, _options_).
    sec-temporal-totemporalyearmonth step 2.c:
      c. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"month"*, *"monthCode"*, *"year"* »).
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
Temporal.PlainYearMonth.from({ year: 2000, month: 5, calendar });

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
