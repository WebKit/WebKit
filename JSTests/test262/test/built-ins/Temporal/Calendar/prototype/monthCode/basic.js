// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthcode
description: Basic tests for monthCode().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = "M11";
assert.sameValue(iso.monthCode(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.monthCode(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.monthCode(Temporal.PlainYearMonth.from("1994-11")), res, "PlainYearMonth");
assert.sameValue(iso.monthCode(Temporal.PlainMonthDay.from("11-05")), res, "PlainMonthDay");
assert.sameValue(iso.monthCode({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.monthCode("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.monthCode({ year: 2000 }), "property bag with missing properties");
