// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
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
// ZonedDateTime.until() -> DifferenceZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()

const later1 = new Temporal.ZonedDateTime(1_213_200_000_000_000n, timeZone, calendar);
earlier.until(later1, { largestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 1, "basic difference with largestUnit >days");

// Difference with rounding, with smallestUnit a calendar unit.
// The calls come from these paths:
// ZonedDateTime.until() ->
//   DifferenceZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//   RoundDuration ->
//     MoveRelativeZonedDateTime -> AddZonedDateTime -> calendar.dateAdd()
//     MoveRelativeDate -> calendar.dateAdd()

calendar.dateAddCallCount = 0;

earlier.until(later1, { smallestUnit: "weeks" });
assert.sameValue(calendar.dateAddCallCount, 3, "rounding difference with calendar smallestUnit");
