// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplainyearmonth
description: >
    Calendar.yearMonthFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.PlainDate(2000, 5, 2, calendar);
instance.toPlainYearMonth();
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should have been called on the calendar");
