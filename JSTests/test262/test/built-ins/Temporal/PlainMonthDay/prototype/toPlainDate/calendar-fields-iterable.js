// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plainmonthday.prototype.toplaindate step 4:
      4. Let _receiverFieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"monthCode"* »).
    sec-temporal.plainmonthday.prototype.toplaindate step 7:
      7. Let _inputFieldNames_ be ? CalendarFields(_calendar_, « *"year"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected1 = [
  "day",
  "monthCode",
];
const expected2 = [
  "year",
];

const calendar = TemporalHelpers.calendarFieldsIterable();
const monthday = new Temporal.PlainMonthDay(5, 2, calendar);
monthday.toPlainDate({ year: 1997 });

assert.sameValue(calendar.fieldsCallCount, 2, "fields() method called twice");
assert.compareArray(calendar.fieldsCalledWith[0], expected1, "fields() method called first time with correct args");
assert.compareArray(calendar.fieldsCalledWith[1], expected2, "fields() method called second time with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole first iterable");
assert(calendar.iteratorExhausted[1], "iterated through the whole second iterable");
