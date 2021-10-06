// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindate
description: Fast path for converting other Temporal objects to Temporal.Calendar by reading internal slots
info: |
    sec-temporal.now.plaindate step 1:
      1. Let _dateTime_ be ? SystemDateTime(_temporalTimeZoneLike_, _calendar_).
    sec-temporal-systemdatetime step 3:
      3. Let _calendar_ be ? ToTemporalCalendar(_calendarLike_).
    sec-temporal-totemporalcalendar step 1.a:
      a. If _temporalCalendarLike_ has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, then
        i. Return _temporalCalendarLike_.[[Calendar]].
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkToTemporalCalendarFastPath((temporalObject, calendar) => {
  const result = Temporal.Now.plainDate(temporalObject);
  assert.sameValue(result.calendar, calendar, "Temporal object coerced to calendar");
});
