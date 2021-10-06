// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: Basic tests for day().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 5;
assert.sameValue(iso.day(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.day(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.day(Temporal.PlainMonthDay.from("11-05")), res, "PlainMonthDay");
assert.sameValue(iso.day({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.day("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.day({ year: 2000 }), "property bag with missing properties");
