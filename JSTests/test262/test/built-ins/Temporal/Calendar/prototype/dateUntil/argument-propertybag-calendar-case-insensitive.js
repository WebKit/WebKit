// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const calendar = "IsO8601";

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.dateUntil(arg, new Temporal.PlainDate(1976, 11, 19));
TemporalHelpers.assertDuration(result1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive (first argument)");
const result2 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 19), arg);
TemporalHelpers.assertDuration(result2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "Calendar is case-insensitive (second argument)");
