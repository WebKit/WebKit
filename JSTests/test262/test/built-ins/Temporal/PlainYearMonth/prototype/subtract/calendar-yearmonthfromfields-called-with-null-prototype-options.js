// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: >
    Calendar.yearMonthFromFields method is called with a null-prototype object
    as the options value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckOptionsPrototypePollution();
const instance = new Temporal.PlainYearMonth(2019, 6, calendar);
const argument = new Temporal.Duration(1, 1);
instance.subtract(argument);
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should be called on the calendar");
