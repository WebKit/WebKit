// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd add duration with years only calculate correctly.
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

let p1y = new Temporal.Duration(1);
let p4y = new Temporal.Duration(4);

TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-29", p1y), 2021, 2, "M02", 28,
    "add one year on Feb 29");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2020-02-29", p4y), 2024, 2, "M02", 29,
    "add four years on Feb 29");
TemporalHelpers.assertPlainDate(
    cal.dateAdd("2021-07-16", p1y), 2022, 7, "M07", 16,
    "add one year on other date");
