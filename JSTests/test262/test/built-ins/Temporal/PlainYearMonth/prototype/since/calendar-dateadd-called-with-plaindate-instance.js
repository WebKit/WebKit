// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: >
    relativeTo parameters that are not ZonedDateTime or undefined, are always
    converted to PlainDate for observable calendar calls
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddPlainDateInstance();
const instance = new Temporal.PlainYearMonth(1970, 1, calendar);
instance.since(new Temporal.PlainYearMonth(2000, 5, calendar), { smallestUnit: "year" });
assert(calendar.dateAddCallCount > 0, "assertions in calendar.dateAdd() should have been tested");
