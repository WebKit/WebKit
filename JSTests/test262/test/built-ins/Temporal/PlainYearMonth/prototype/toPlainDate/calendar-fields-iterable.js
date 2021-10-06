// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.toplaindate
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainyearmonth.prototype.toplaindate step 5:
      5. Let _receiverFieldNames_ be ? CalendarFields(_calendar_, « *"monthCode"*, *"year"* »).
    sec-temporal.plainyearmonth.prototype.toplaindate step 7:
      7. Let _inputFieldNames_ be ? CalendarFields(_calendar_, « *"day"* »).
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
  "day",
];

const calendar = TemporalHelpers.calendarFieldsIterable();
const yearmonth = new Temporal.PlainYearMonth(2000, 5, calendar);
yearmonth.toPlainDate({ day: 15 });

assert.sameValue(calendar.fieldsCallCount, 2, "fields() method called twice");
assert.compareArray(calendar.fieldsCalledWith[0], expected1, "fields() method called first time with correct args");
assert.compareArray(calendar.fieldsCalledWith[1], expected2, "fields() method called second time with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole first iterable");
assert(calendar.iteratorExhausted[1], "iterated through the whole second iterable");
