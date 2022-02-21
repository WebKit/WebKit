// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.plaintime.prototype.tozoneddatetime step 5:
      3. Let _temporalDate_ be ? ToTemporalDate(_temporalDateLike_).
    sec-temporal-totemporaldate step 2.c:
      c. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"month"*, *"monthCode"*, *"year"* »).
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

const time = new Temporal.PlainTime(13, 3);
const calendar = TemporalHelpers.calendarFieldsIterable();
time.toZonedDateTime({ plainDate: { year: 2000, month: 5, day: 3, calendar }, timeZone: "UTC" });

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
