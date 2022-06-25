// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with days and calculate correctly..
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p10d = new Temporal.Duration(0,0,0,10);

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-16", p10d), 2021, 7, "M07", 26,
    "add 10 days and result into the same  month");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-26", p10d), 2021, 8, "M08", 5,
    "add 10 days and result into next month");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-12-26", p10d), 2022, 1, "M01", 5,
    "add 10 days and result into next year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-26", p10d), 2020, 3, "M03", 7,
    "add 10 days from a leap year in Feb and result into March");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-26", p10d), 2021, 3, "M03", 8,
    "add 10 days from a non leap year in Feb and result into March");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-19", p10d), 2020, 2, "M02", 29,
    "add 10 days from a leap year in Feb 19 and result into Feb 29");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-19", p10d), 2021, 3, "M03", 1,
    "add 10 days from a non leap year in Feb 19 and result into March 1");
