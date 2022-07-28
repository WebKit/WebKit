// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: >
    Calendar.dateUntil method is called with a null-prototype object as the
    options value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckOptionsPrototypePollution();
const instance = new Temporal.PlainYearMonth(2000, 5, calendar);
const argument = new Temporal.PlainYearMonth(2022, 6, calendar);
instance.since(argument);
assert.sameValue(calendar.dateUntilCallCount, 1, "dateUntil should have been called on the calendar");
