// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: Basic tests for monthsInYear().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 12;
assert.sameValue(iso.monthsInYear(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.monthsInYear(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.monthsInYear(Temporal.PlainYearMonth.from("1994-11")), res, "PlainYearMonth");
assert.sameValue(iso.monthsInYear({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.monthsInYear("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.monthsInYear({ year: 2000 }), "property bag with missing properties");
