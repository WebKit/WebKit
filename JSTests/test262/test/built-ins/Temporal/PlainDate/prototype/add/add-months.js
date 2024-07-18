// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Add duration with months and calculate correctly
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/

let p5m = new Temporal.Duration(0, 5);

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-07-16").add(p5m), 2021, 12, "M12", 16,
    "add five months and result in the same year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-08-16").add(p5m), 2022, 1, "M01", 16,
    "add five months and result in the next year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-10-31").add(p5m), 2022, 3, "M03", 31,
    "add five months and result in the next year in end of month");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2019-10-01").add(p5m), 2020, 3, "M03", 1,
    "add five months and result in the next year in end of month on leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-09-30").add(p5m), 2022, 2, "M02", 28,
    "add five months and result in the nexdt year and constrain to Feb 28");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2019-09-30").add(p5m), 2020, 2, "M02", 29,
    "add five months and result in the nexdt year and constrain to Feb 29 on leap year");
