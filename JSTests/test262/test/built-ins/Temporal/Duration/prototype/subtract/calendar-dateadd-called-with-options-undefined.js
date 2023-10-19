// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: >
    BuiltinTimeZoneGetInstantFor calls Calendar.dateAdd with undefined as the
    options value
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddUndefinedOptions();
const timeZone = TemporalHelpers.oneShiftTimeZone(new Temporal.Instant(0n), 3600e9);
const instance = new Temporal.Duration(1, 1, 1, 1);
instance.subtract(new Temporal.Duration(-1, -1, -1, -1), { relativeTo: new Temporal.ZonedDateTime(0n, timeZone, calendar) });
assert.sameValue(calendar.dateAddCallCount, 3);
