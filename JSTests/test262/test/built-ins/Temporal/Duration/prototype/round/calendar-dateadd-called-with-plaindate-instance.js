// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    relativeTo parameters that are not ZonedDateTime or undefined, are always
    converted to PlainDate for observable calendar calls
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddPlainDateInstance();
const instance = new Temporal.Duration(1, 1, 1, 1);
const relativeTo = new Temporal.PlainDate(2000, 1, 1, calendar);
calendar.specificPlainDate = relativeTo;
instance.round({ largestUnit: "days", relativeTo });
assert(calendar.dateAddCallCount > 0, "assertions in calendar.dateAdd() should have been tested");
