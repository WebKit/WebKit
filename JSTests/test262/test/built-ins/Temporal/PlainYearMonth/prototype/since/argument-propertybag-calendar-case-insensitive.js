// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2019, 6);

const calendar = "IsO8601";

let arg = { year: 2019, monthCode: "M06", calendar };
const result1 = instance.since(arg);
TemporalHelpers.assertDuration(result1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive");

arg = { year: 2019, monthCode: "M06", calendar: { calendar } };
const result2 = instance.since(arg);
TemporalHelpers.assertDuration(result2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive (nested property)");
