// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: >
    Calendar.yearMonthFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.PlainYearMonth(2000, 5);
const arg = { year: 2000, month: 5, calendar };
instance.until(arg);
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1, "yearMonthFromFields should be called on the property bag's calendar");
