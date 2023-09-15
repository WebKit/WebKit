// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Verify the result of calendar.fields() is treated correctly.
info: |
    sec-temporal.timezone.prototype.getinstantfor step 3:
      3. Set _dateTime_ to ? ToTemporalDateTime(_dateTime_).
    sec-temporal-totemporaldatetime step 2.c:
      c. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"hour"*, *"microsecond"*, *"millisecond"*, *"minute"*, *"month"*, *"monthCode"*, *"nanosecond"*, *"second"*, *"year"* »).
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

const calendar = TemporalHelpers.calendarFieldsIterable();
const timeZone = new Temporal.TimeZone("UTC");
timeZone.getInstantFor({ year: 2000, month: 5, day: 2, calendar });

assert.sameValue(calendar.fieldsCallCount, 1, "fields() method called once");
assert.compareArray(calendar.fieldsCalledWith[0], expected, "fields() method called with correct args");
assert(calendar.iteratorExhausted[0], "iterated through the whole iterable");
