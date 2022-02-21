// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: Basic tests for month().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 11;
assert.sameValue(iso.month(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.month(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.month(Temporal.PlainYearMonth.from("1994-11")), res, "PlainYearMonth");
assert.sameValue(iso.month({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.month("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.month({ year: 2000 }), "property bag with missing properties");
assert.throws(TypeError, () => iso.month(Temporal.PlainMonthDay.from("11-05")), "PlainMonthDay");
