// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: Fast path for converting other Temporal objects to Temporal.Calendar by reading internal slots
info: |
    sec-temporal.plaintime.from step 4:
      4. Return ? ToTemporalTime(_temporalTime_, _overflow_).
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
  assert.throws(RangeError, () => Temporal.PlainTime.from({ hour: 12, minute: 34, second: 56, calendar: temporalObject }));
});
