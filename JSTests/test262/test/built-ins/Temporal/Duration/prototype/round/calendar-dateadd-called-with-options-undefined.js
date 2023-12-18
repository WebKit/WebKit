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
//   RoundDuration ->
//     MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//     MoveRelativeDate -> calendar.dateAdd()
//   BalanceDurationRelative ->
//     MoveRelativeDate -> calendar.dateAdd() (2x)
//     calendar.dateAdd()

const instance1 = new Temporal.Duration(1, 1, 1, 1, 1);
instance1.round({ smallestUnit: "weeks", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 5, "rounding with calendar smallestUnit");

// Rounding with a non-default largestUnit to cover the path in
// UnbalanceDurationRelative where larger units are converted into smaller
// units; and with a smallestUnit larger than days to cover the path in
// RoundDuration where days are converted into larger units.
// The calls come from these paths:
// Duration.round() ->
//   UnbalanceDurationRelative -> MoveRelativeDate -> calendar.dateAdd()
//   RoundDuration ->
//     MoveRelativeDate -> calendar.dateAdd() (5x)
//   BalanceDurationRelative
//     MoveRelativeDate -> calendar.dateAdd()
//   MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

const instance2 = new Temporal.Duration(0, 1, 1, 1);
instance2.round({ largestUnit: "weeks", smallestUnit: "weeks", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 8, "rounding with non-default largestUnit and calendar smallestUnit");

// Rounding with smallestUnit days only.
// The calls come from these paths:
// Duration.round() ->
//   RoundDuration ->
//     MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//   BalanceDurationRelative ->
//     MoveRelativeDate -> calendar.dateAdd() (2x)
//     calendar.dateAdd()

calendar.dateAddCallCount = 0;

const instance3 = new Temporal.Duration(1, 1, 1, 1, 1);
instance3.round({ smallestUnit: "days", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 4, "rounding with days smallestUnit");
