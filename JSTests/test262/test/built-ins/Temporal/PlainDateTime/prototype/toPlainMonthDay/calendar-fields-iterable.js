// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.toplainmonthday
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plaindatetime.prototype.toplainmonthday step 4:
      4. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"monthCode"* »).
    sec-temporal-calendarfields step 4:
      4. Let _result_ be ? IterableToList(_fieldsArray_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "day",
  "monthCode",
];

const calendar = TemporalHelpers.calendarFieldsIterable();
const datetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
datetime.toPlainMonthDay();

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
