// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: Basic tests for weekOfYear().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 44;
assert.sameValue(iso.weekOfYear(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.weekOfYear(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.weekOfYear({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.weekOfYear("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.weekOfYear({ year: 2000 }), "property bag with missing properties");
