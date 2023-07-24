// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Duration addition to PlainYearMonth calls Calendar.dateAdd the right number of times
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddPlainDateInstance();
const instance = new Temporal.PlainYearMonth(1983, 3, calendar);
TemporalHelpers.assertPlainYearMonth(instance.add({days: 31}), 1983, 4, 'M04', "Adding 31 days to march in is8601 calendar")
assert.sameValue(calendar.dateAddCallCount, 1, "dateAdd called once with positive add");

calendar.dateAddCallCount = 0;
TemporalHelpers.assertPlainYearMonth(instance.add({days: -31}), 1983, 2, 'M02', "Adding -31 days to march in is8601 calendar")
assert.sameValue(calendar.dateAddCallCount, 3, "dateAdd called 3 times with negative add");
