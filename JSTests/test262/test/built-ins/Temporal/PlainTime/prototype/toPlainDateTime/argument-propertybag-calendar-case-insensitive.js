// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const calendar = "IsO8601";

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.toPlainDateTime(arg);
TemporalHelpers.assertPlainDateTime(result1, 1976, 11, "M11", 18, 12, 34, 56, 987, 654, 321, "Calendar is case-insensitive");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = instance.toPlainDateTime(arg);
TemporalHelpers.assertPlainDateTime(result2, 1976, 11, "M11", 18, 12, 34, 56, 987, 654, 321, "Calendar is case-insensitive (nested property)");
