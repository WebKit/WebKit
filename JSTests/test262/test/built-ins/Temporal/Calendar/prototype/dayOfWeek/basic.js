// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dayofweek
description: Basic tests for dayOfWeek().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 6;
assert.sameValue(iso.dayOfWeek(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.dayOfWeek(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.dayOfWeek({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.dayOfWeek("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.dayOfWeek({ year: 2000 }), "property bag with missing properties");
