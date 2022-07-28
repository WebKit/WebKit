// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: >
    Calendar.dateFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.PlainYearMonth(2000, 5, calendar);
instance.add(new Temporal.Duration(1));
assert.sameValue(calendar.dateFromFieldsCallCount, 1, "dateFromFields should have been called on the calendar");
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should have been called on the calendar");
