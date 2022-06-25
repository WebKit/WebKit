// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with months and weeks and calculate correctly.
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p2m3w = new Temporal.Duration(0,2,3);

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-29", p2m3w), 2020, 5, "M05", 20,
    "add two months 3 weeks from Feb 29 of a leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-28", p2m3w), 2020, 5, "M05", 19,
    "add two months 3 weeks from Feb 28 of a leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-28", p2m3w), 2021, 5, "M05", 19,
    "add two months 3 weeks from Feb 28 of a non leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-12-28", p2m3w), 2021, 3, "M03", 21,
    "add two months 3 weeks from end of year to non leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2019-12-28", p2m3w), 2020, 3, "M03", 20,
    "add two months 3 weeks from end of year to leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2019-10-28", p2m3w), 2020, 1, "M01", 18,
    "add two months 3 weeks and cause roll into a new year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2019-10-31", p2m3w), 2020, 1, "M01", 21,
    "add two months 3 weeks and cause roll into a new year");
