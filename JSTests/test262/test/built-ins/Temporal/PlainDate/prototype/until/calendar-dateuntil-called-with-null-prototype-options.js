// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: >
    Calendar.dateUntil method is called with a null-prototype object as the
    options value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckOptionsPrototypePollution();
const instance = new Temporal.PlainDate(2000, 5, 2, calendar);
const argument = new Temporal.PlainDate(2022, 6, 14, calendar);
instance.until(argument, { largestUnit: "months" });
assert.sameValue(calendar.dateUntilCallCount, 1, "dateUntil should have been called on the calendar");
