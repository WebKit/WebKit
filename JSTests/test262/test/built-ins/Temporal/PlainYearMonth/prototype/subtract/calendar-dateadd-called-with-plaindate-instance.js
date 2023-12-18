// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Duration subtraction from PlainYearMonth calls Calendar.dateAdd the right number of times
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddPlainDateInstance();
const instance = new Temporal.PlainYearMonth(1983, 3, calendar);
TemporalHelpers.assertPlainYearMonth(instance.subtract({weeks: 5}), 1983, 2, 'M02', "Removing 5 weeks from March in is8601 calendar")
assert.sameValue(calendar.dateAddCallCount, 2, "dateAdd called 2 times with positive subtract");

calendar.dateAddCallCount = 0;
TemporalHelpers.assertPlainYearMonth(instance.subtract({weeks: -5}), 1983, 4, 'M04', "Removing -5 weeks from March in is8601 calendar")
assert.sameValue(calendar.dateAddCallCount, 1, "dateAdd called once with negative subtract");
