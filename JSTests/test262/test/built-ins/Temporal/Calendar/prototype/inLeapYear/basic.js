// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: Basic tests for inLeapYear().
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
let res = false;

assert.sameValue(iso.inLeapYear(new Temporal.PlainDate(1994, 11, 5)), res, "PlainDate");
assert.sameValue(iso.inLeapYear(new Temporal.PlainDateTime(1994, 11, 5, 8, 15, 30)), res, "PlainDateTime");
assert.sameValue(iso.inLeapYear(new Temporal.PlainYearMonth(1994, 11)), res, "PlainYearMonth");
assert.sameValue(iso.inLeapYear({ year: 1994, month: 11, day: 5 }), res, "property bag");
assert.sameValue(iso.inLeapYear("1994-11-05"), res, "string");

res = true;
assert.sameValue(iso.inLeapYear(new Temporal.PlainDate(1996, 7, 15)), res, "PlainDate in leap year");
assert.sameValue(iso.inLeapYear(new Temporal.PlainDateTime(1996, 7, 15, 5, 30, 13)), res, "PlainDateTime in leap year");
assert.sameValue(iso.inLeapYear(new Temporal.PlainYearMonth(1996, 7)), res, "PlainYearMonth in leap year");
assert.sameValue(iso.inLeapYear({ year: 1996, month: 7, day: 15 }), res, "property bag in leap year");
assert.sameValue(iso.inLeapYear("1996-07-15"), res, "string in leap year");

assert.throws(TypeError, () => iso.inLeapYear({ year: 2000 }), "property bag with missing properties");
