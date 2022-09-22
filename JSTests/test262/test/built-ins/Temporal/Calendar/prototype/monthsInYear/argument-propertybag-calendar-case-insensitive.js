// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const calendar = "IsO8601";

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result = instance.monthsInYear(arg);
assert.sameValue(result, 12, `Calendar created from string "${calendar}"`);
