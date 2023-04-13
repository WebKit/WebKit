// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const calendar = "IsO8601";

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.weekOfYear(arg);
assert.sameValue(result1, 47, "Calendar is case-insensitive");

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = instance.weekOfYear(arg);
assert.sameValue(result2, 47, "Calendar is case-insensitive (nested property)");
