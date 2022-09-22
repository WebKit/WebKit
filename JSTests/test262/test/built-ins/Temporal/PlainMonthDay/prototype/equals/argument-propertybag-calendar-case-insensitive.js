// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: The calendar name is case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.PlainMonthDay(11, 18);

const calendar = "IsO8601";

const arg = { monthCode: "M11", day: 18, calendar };
const result = instance.equals(arg);
assert.sameValue(result, true, `Calendar created from string "${calendar}"`);
