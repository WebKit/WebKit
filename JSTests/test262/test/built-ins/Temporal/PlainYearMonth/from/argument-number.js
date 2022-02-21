// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A number argument is stringified
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainYearMonth = Temporal.PlainYearMonth.from(201906);
TemporalHelpers.assertPlainYearMonth(plainYearMonth, 2019, 6, "M06");
const fields = plainYearMonth.getISOFields();
assert.sameValue(fields.calendar.id, "iso8601");
assert.sameValue(fields.isoDay, 1, "isoDay");
assert.sameValue(fields.isoMonth, 6, "isoMonth");
assert.sameValue(fields.isoYear, 2019, "isoYear");
