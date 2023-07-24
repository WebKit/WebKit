// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: >
    Calendar.yearMonthFromFields method is called with undefined as the options
    value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
let instance = new Temporal.PlainYearMonth(2000, 5, calendar);
instance.until({ year: 2000, month: 6, calendar });
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 1);
