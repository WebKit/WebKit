// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinmonth
description: Basic tests for daysInMonth().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 30;
assert.sameValue(iso.daysInMonth(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.daysInMonth(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.daysInMonth({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.daysInMonth("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.daysInMonth({ year: 2000 }), "property bag with missing properties");
