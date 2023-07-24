// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: >
    Calendar.yearMonthFromFields method is called with undefined as the options
    value when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
Temporal.PlainYearMonth.compare({ year: 2000, month: 5, calendar }, { year: 2000, month: 6, calendar });
assert.sameValue(calendar.yearMonthFromFieldsCallCount, 2);
