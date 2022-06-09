// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A string is parsed into the correct object when passed as the argument
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainYearMonth = Temporal.PlainYearMonth.from("1970-12");
TemporalHelpers.assertPlainYearMonth(plainYearMonth, 1970, 12, "M12");
const fields = plainYearMonth.getISOFields();
assert.sameValue(fields.calendar.id, "iso8601");
assert.sameValue(fields.isoDay, 1, "isoDay");
assert.sameValue(fields.isoMonth, 12, "isoMonth");
assert.sameValue(fields.isoYear, 1970, "isoYear");
