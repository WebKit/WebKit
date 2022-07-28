// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: >
    Calendar.yearMonthFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const arg1 = { year: 2000, month: 5, calendar };
const arg2 = new Temporal.PlainYearMonth(2019, 6);

Temporal.PlainYearMonth.compare(arg1, arg2);
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should be called on the property bag's calendar (first argument)");

calendar.yearMonthFromFieldsCallCount = 0;

Temporal.PlainYearMonth.compare(arg2, arg1);
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should be called on the property bag's calendar (second argument)");
