// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    BuiltinTimeZoneGetInstantFor calls Calendar.dateAdd with undefined as the
    options value
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddUndefinedOptions();
const timeZone = TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 3600e9);
const relativeTo = new Temporal.ZonedDateTime(0n, timeZone, calendar);

// Rounding with smallestUnit a calendar unit.
// The calls come from these paths:
// Duration.round() ->
//   AddZonedDateTime -> calendar.dateAdd()
//   DifferenceZonedDateTimeWithRounding -> RoundRelativeDuration -> NudgeToCalendarUnit ->
//       AddDateTime -> calendar.dateAdd() (2x)

const instance = new Temporal.Duration(1, 1, 1, 1, 1);
instance.round({ smallestUnit: "weeks", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 3, "rounding with calendar smallestUnit");

// Rounding with smallestUnit days only.
// The calls come from these paths:
// Duration.round() ->
//   AddZonedDateTime -> calendar.dateAdd()
//   DifferenceZonedDateTimeWithRounding ->
//     RoundDuration -> MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//     BalanceDateDurationRelative -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

instance.round({ smallestUnit: "days", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 3, "rounding with days smallestUnit");
