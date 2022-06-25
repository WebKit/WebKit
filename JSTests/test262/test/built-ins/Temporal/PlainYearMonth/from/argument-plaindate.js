// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A PlainYearMonth argument is cloned
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = Temporal.PlainDate.from("1976-11-18");
const plainYearMonth = Temporal.PlainYearMonth.from(plainDate);
TemporalHelpers.assertPlainYearMonth(plainYearMonth, 1976, 11, "M11");
const fields = plainYearMonth.getISOFields();
assert.sameValue(fields.calendar.id, "iso8601");
assert.sameValue(fields.isoDay, 1, "isoDay");
assert.sameValue(fields.isoMonth, 11, "isoMonth");
assert.sameValue(fields.isoYear, 1976, "isoYear");
