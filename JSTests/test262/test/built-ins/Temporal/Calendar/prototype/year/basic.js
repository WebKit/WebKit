// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.year
description: Basic tests for year().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 1994;
assert.sameValue(iso.year(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.year(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.year(Temporal.PlainYearMonth.from("1994-11")), res, "PlainYearMonth");
assert.sameValue(iso.year({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.year("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.year({ month: 5 }), "property bag with missing properties");
