// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const calendar = "IsO8601";

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" });
assert.sameValue(result1.epochNanoseconds, 217_168_496_987_654_321n, "Calendar is case-insensitive");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" });
assert.sameValue(result2.epochNanoseconds, 217_168_496_987_654_321n, "Calendar is case-insensitive (nested property)");
