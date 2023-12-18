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
// The call comes from this path:
// ZonedDateTime.since() -> DifferenceZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()

const later1 = new Temporal.ZonedDateTime(1_213_200_000_000_000n, timeZone, calendar);
later1.since(earlier, { largestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 1, "basic difference with largestUnit >days");

// Difference with rounding, with smallestUnit a calendar unit.
// The calls come from these paths:
// ZonedDateTime.since() ->
//   DifferenceZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//   RoundDuration ->
//     MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//     MoveRelativeDate -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

later1.since(earlier, { smallestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 3, "rounding difference with calendar smallestUnit");
