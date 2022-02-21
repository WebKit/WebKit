// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: Basic tests for inLeapYear().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const res = false;
assert.sameValue(iso.inLeapYear(Temporal.PlainDate.from("1994-11-05")), res, "PlainDate");
assert.sameValue(iso.inLeapYear(Temporal.PlainDateTime.from("1994-11-05T08:15:30")), res, "PlainDateTime");
assert.sameValue(iso.inLeapYear(Temporal.PlainYearMonth.from("1994-11")), res, "PlainYearMonth");
assert.sameValue(iso.inLeapYear({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.inLeapYear("1994-11-05"), res, "string");
assert.throws(TypeError, () => iso.inLeapYear({ year: 2000 }), "property bag with missing properties");
