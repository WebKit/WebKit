// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with weeks and days and calculate correctly.
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p2w3d = new Temporal.Duration(0,0,2,3);
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-29", p2w3d), 2020, 3, "M03", 17,
    "add 2 weeks and 3 days (17 days) from Feb 29 in a leap year and cause rolling into March");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-28", p2w3d), 2021, 3, "M03", 17,
    "add 2 weeks and 3 days (17 days) from Feb and cause rolling into March in a non leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-28", p2w3d), 2020, 3, "M03", 16,
    "add 2 weeks and 3 days (17 days) from Feb and cause rolling into March in a leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-12-28", p2w3d), 2021, 1, "M01", 14,
    "add 2 weeks and 3 days (17 days) and cause rolling into a new year");
