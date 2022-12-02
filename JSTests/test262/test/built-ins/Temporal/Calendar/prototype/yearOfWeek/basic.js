// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: Basic tests for yearOfWeek().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 1994;
assert.sameValue(iso.yearOfWeek(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.yearOfWeek(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.yearOfWeek({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.yearOfWeek("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.yearOfWeek({ year: 2000 }), "property bag with missing properties");
