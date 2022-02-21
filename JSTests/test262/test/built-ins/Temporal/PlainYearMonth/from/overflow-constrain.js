// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Reject value for overflow option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const propertyBag = { year: 2000, month: 13 };
const plainYearMonth = Temporal.PlainYearMonth.from(propertyBag, { overflow: "constrain" });
TemporalHelpers.assertPlainYearMonth(plainYearMonth, 2000, 12, "M12", "default overflow is constrain");

