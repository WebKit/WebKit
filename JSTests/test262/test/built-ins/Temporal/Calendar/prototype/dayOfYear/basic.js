// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dayofyear
description: Basic tests for dayOfYear().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = 309;
assert.sameValue(iso.dayOfYear(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.dayOfYear(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.dayOfYear({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.dayOfYear("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.dayOfYear({ year: 2000 }), "property bag with missing properties");
