// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Add duration with years and weeks and calculate correctly
info: |
  8. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]], duration.[[Years]], duration.[[Months]], duration.[[Weeks]], duration.[[Days]], overflow).
features: [Temporal]
includes: [temporalHelpers.js]
---*/

let p1y2w = new Temporal.Duration(1,0,2);

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2020-02-28").add(p1y2w), 2021, 3, "M03", 14,
    "add 1 year and 2 weeks to Feb 28 and cause roll into March in a non leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2020-02-29").add(p1y2w), 2021, 3, "M03", 14,
    "add 1 year and 2 weeks to Feb 29 and cause roll into March in a non leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2019-02-28").add(p1y2w), 2020, 3, "M03", 13,
    "add 1 year and 2 weeks to Feb 28 and cause roll into March in a leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2021-02-28").add(p1y2w), 2022, 3, "M03", 14,
    "add 1 year and 2 weeks to Feb 28 and cause roll into March in a non leap year");
TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from("2020-12-28").add(p1y2w), 2022, 1, "M01", 11,
    "add 1 year and 2 weeks and cause roll into a new year");
