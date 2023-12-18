// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
    BuiltinTimeZoneGetInstantFor calls Calendar.dateAdd with undefined as the
    options value
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddUndefinedOptions();
const timeZone = TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 3600e9);
const relativeTo = new Temporal.ZonedDateTime(0n, timeZone, calendar);

// Total of a calendar unit where larger calendar units have to be converted
// down, to cover the path that goes through UnbalanceDurationRelative
// The calls come from these paths:
// Duration.total() ->
//   UnbalanceDurationRelative -> MoveRelativeDate -> calendar.dateAdd() (3x)
//   BalanceDuration ->
//     AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()

const instance1 = new Temporal.Duration(1, 1, 1, 1, 1);
instance1.total({ unit: "days", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 3, "converting larger calendar units down");

// Total of a calendar unit where smaller calendar units have to be converted
// up, to cover the path that goes through MoveRelativeZonedDateTime
// The calls come from these paths:
// Duration.total() ->
//   MoveRelativeZonedDateTime -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//   BalanceDuration ->
//     AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//   RoundDuration ->
//     MoveRelativeDate -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

const instance2 = new Temporal.Duration(0, 0, 1, 1);
instance2.total({ unit: "weeks", relativeTo });
assert.sameValue(calendar.dateAddCallCount, 3, "converting smaller calendar units up");
