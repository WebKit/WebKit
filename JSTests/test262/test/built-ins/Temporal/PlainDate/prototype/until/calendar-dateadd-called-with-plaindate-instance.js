// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: >
    relativeTo parameters that are not ZonedDateTime or undefined, are always
    converted to PlainDate for observable calendar calls
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarDateAddPlainDateInstance();
const instance = new Temporal.PlainDate(1970, 1, 1, calendar);
calendar.specificPlainDate = instance;
instance.until(new Temporal.PlainDate(2000, 5, 2, calendar), { smallestUnit: "month" });
assert(calendar.dateAddCallCount > 0, "assertions in calendar.dateAdd() should have been tested");
