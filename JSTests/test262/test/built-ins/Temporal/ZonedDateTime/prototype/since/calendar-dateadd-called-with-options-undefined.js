// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: >
    BuiltinTimeZoneGetInstantFor calls Calendar.dateAdd with undefined as the
    options value
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddUndefinedOptions();
const timeZone = TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 3600e9);
const earlier = new Temporal.ZonedDateTime(0n, timeZone, calendar);

// Basic difference with largestUnit larger than days.
// The calls come from these paths:
// ZonedDateTime.since() -> DifferenceZonedDateTime ->
//   AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//   NanosecondsToDays -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()

const later1 = new Temporal.ZonedDateTime(1_213_200_000_000_000n, timeZone, calendar);
later1.since(earlier, { largestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 2, "basic difference with largestUnit >days");

// Basic difference with largestUnit equal to days, to cover the second path in
// AddZonedDateTime.
// The calls come from these paths:
// ZonedDateTime.since() -> DifferenceZonedDateTime -> NanosecondsToDays -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd() (2x)

calendar.dateAddCallCount = 0;

later1.since(earlier, { largestUnit: "days" });
assert.sameValue(calendar.dateAddCallCount, 2, "basic difference with largestUnit days");

// Difference with rounding, with smallestUnit a calendar unit.
// The calls come from these paths:
// ZonedDateTime.since() ->
//   DifferenceZonedDateTime ->
//     AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//     NanosecondsToDays -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//   RoundDuration ->
//     MoveRelativeZonedDateTime -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//     NanosecondsToDays -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd()
//     MoveRelativeDate -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

later1.since(earlier, { smallestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 5, "rounding difference with calendar smallestUnit");

// Difference with rounding, with smallestUnit a non-calendar unit, and having
// the resulting time difference be longer than a calendar day, covering the
// paths that go through AdjustRoundedDurationDays. (The path through
// AdjustRoundedDurationDays -> AddDuration that's covered in the corresponding
// test in until() only happens in one direction.)
// The calls come from these paths:
// ZonedDateTime.since() ->
//   DifferenceZonedDateTime -> NanosecondsToDays -> AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd() (3x)
//   AdjustRoundedDurationDays ->
//     AddZonedDateTime -> BuiltinTimeZoneGetInstantFor -> calendar.dateAdd() (3x)

calendar.dateAddCallCount = 0;

const later2 = new Temporal.ZonedDateTime(86_399_999_999_999n, timeZone, calendar);
later2.since(earlier, { largestUnit: "days", smallestUnit: "hours", roundingMode: "ceil" });
assert.sameValue(calendar.dateAddCallCount, 6, "rounding difference with non-calendar smallestUnit and time difference longer than a calendar day");
