// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.until
description: Fast path for converting other Temporal objects to Temporal.Calendar by reading internal slots
info: |
    sec-temporal.plaintime.prototype.until step 3:
      3. Set _other_ to ? ToTemporalTime(_other_).
    sec-temporal-totemporaltime step 3.d:
      d. If _calendar_ is not *undefined*, then
        i. Set _calendar_ to ? ToTemporalCalendar(_calendar_).
        ii. If ? ToString(_calendar_) is not *"iso8601"*, then
          1. Throw a *RangeError* exception.
    sec-temporal-totemporalcalendar step 1.a:
      a. If _temporalCalendarLike_ has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, then
        i. Return _temporalCalendarLike_.[[Calendar]].
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkToTemporalCalendarFastPath((temporalObject) => {
  const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
  assert.throws(RangeError, () => time.until({ hour: 12, minute: 30, calendar: temporalObject }));
});
