// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with years and months and calculate correctly.
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p1y2m = new Temporal.Duration(1,2);

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-16", p1y2m), 2022, 9, "M09", 16,
    "add one year and 2 months");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-11-30", p1y2m), 2023, 1, "M01", 30,
    "add one year and 2 months roll into a new year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-12-31", p1y2m), 2023, 2, "M02", 28,
    "add one year and 2 months roll into a new year and constrain in Feb 28 of a non leap year");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2022-12-31", p1y2m), 2024, 2, "M02", 29,
    "add one year and 2 months roll into a new year and constrain in Feb 29 of a leap year");
