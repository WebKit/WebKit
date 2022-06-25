// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with weeks and calculate correctly.
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p1w = new Temporal.Duration(0,0,1);
let p6w = new Temporal.Duration(0,0,6);

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-19", p1w), 2021, 2, "M02", 26,
    "add one week in Feb");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-27", p1w), 2021, 3, "M03", 6,
    "add one week in Feb and result in March");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-27", p1w), 2020, 3, "M03", 5,
    "add one week in Feb and result in March in a leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-12-24", p1w), 2021, 12, "M12", 31,
    "add one week and result in the last day of a year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-12-25", p1w), 2022, 1, "M01", 1,
    "add one week and result in the first day of next year");

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-01-27", p1w), 2021, 2, "M02", 3,
    "add one week and result in next month from a month with 31 days");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-06-27", p1w), 2021, 7, "M07", 4,
    "add one week and result in next month from a month with 30 days");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-27", p1w), 2021, 8, "M08", 3,
    "add one week and result in next month from a month with 31 days");

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-02-19", p6w), 2021, 4, "M04", 2,
    "add six weeks and result in next month from Feb in a non leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-19", p6w), 2020, 4, "M04", 1,
    "add six weeks and result in next month from Feb in a leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-12-24", p6w), 2022, 2, "M02", 4,
    "add six weeks and result in the next year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-01-27", p6w), 2021, 3, "M03", 10,
    "add six weeks and result in the same year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-01-27", p6w), 2020, 3, "M03", 9,
    "add six weeks and result in the same year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-06-27", p6w), 2021, 8, "M08", 8,
    "add six weeks and result in the same year crossing month of 30 and 31 days");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-27", p6w), 2021, 9, "M09", 7,
    "add six weeks and result in the same year crossing month of 31 and 31 days");
