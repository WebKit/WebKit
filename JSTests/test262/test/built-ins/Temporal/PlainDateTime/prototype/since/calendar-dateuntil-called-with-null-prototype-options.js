// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: >
    Calendar.dateUntil method is called with a null-prototype object as the
    options value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckOptionsPrototypePollution();
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
const argument = new Temporal.PlainDateTime(2022, 6, 14, 18, 21, 36, 660, 690, 387, calendar);
instance.since(argument, { largestUnit: "months" });
assert.sameValue(calendar.dateUntilCallCount, 1, "dateUntil should have been called on the calendar");
