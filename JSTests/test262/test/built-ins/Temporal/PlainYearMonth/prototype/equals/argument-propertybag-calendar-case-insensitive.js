// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.equals
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2019, 6);

const calendar = "IsO8601";

let arg = { year: 2019, monthCode: "M06", calendar };
const result1 = instance.equals(arg);
assert.sameValue(result1, true, "Calendar is case-insensitive");

arg = { year: 2019, monthCode: "M06", calendar: { calendar } };
const result2 = instance.equals(arg);
assert.sameValue(result2, true, "Calendar is case-insensitive (nested property)");
