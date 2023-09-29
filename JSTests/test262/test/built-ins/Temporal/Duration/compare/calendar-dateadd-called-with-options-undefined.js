// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: >
    BuiltinTimeZoneGetInstantFor calls Calendar.dateAdd with undefined as the
    options value
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddUndefinedOptions();
const timeZone = TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 3600e9);
const relativeTo = new Temporal.ZonedDateTime(0n, timeZone, calendar);

const duration1 = new Temporal.Duration(0, 0, 1);
const duration2 = new Temporal.Duration(0, 0, 1, 1);
Temporal.Duration.compare(duration1, duration2, { relativeTo });
assert.sameValue(calendar.dateAddCallCount, 2);
// one call for each duration argument to add it to relativeTo
