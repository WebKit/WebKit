// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Add duration with years, months and days and calculate correctly
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/

let p1y2m4d = new Temporal.Duration(1,2,0,4);

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-07-16").add(p1y2m4d), 2022, 9, "M09", 20,
    "add one year two months and 4 days");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-02-27").add(p1y2m4d), 2022, 5, "M05", 1,
    "add one year two months and 4 days and roll into new month from a month of 30 days");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-01-28").add(p1y2m4d), 2022, 4, "M04", 1,
    "add one year two months and 4 days and roll into new month from a month of 31 days");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-02-26").add(p1y2m4d), 2022, 4, "M04", 30,
    "add one year two months and 4 days which roll from March to April in a non leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2023-02-26").add(p1y2m4d), 2024, 4, "M04", 30,
    "add one year two months and 4 days which roll from March to April in a leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-12-30").add(p1y2m4d), 2023, 3, "M03", 4,
    "add one year two months and 4 days which roll month into new year and roll day into March in non leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2022-12-30").add(p1y2m4d), 2024, 3, "M03", 4,
    "add one year two months and 4 days which roll month into new year and roll day into March in leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2022-12-29").add(p1y2m4d), 2024, 3, "M03", 4,
    "add one year two months and 4 days which roll month into new year and roll day into March in leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-07-30").add(p1y2m4d), 2022, 10, "M10", 4,
    "add one year two months and 4 days which roll into a new month from a month with 30 days");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-06-30").add(p1y2m4d), 2022, 9, "M09", 3,
    "add one year two months and 4 days which roll into a new month from a month with 31 days");
