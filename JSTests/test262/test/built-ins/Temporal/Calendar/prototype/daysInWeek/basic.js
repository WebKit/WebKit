// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinweek
description: Basic tests for daysInWeek().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 7;
assert.sameValue(iso.daysInWeek(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.daysInWeek(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.daysInWeek({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.daysInWeek("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.daysInWeek({ year: 2000 }), "property bag with missing properties");
