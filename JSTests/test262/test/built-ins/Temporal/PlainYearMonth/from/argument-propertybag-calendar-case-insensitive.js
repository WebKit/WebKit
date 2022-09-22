// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = "IsO8601";

const arg = { year: 2019, monthCode: "M06", calendar };
const result = Temporal.PlainYearMonth.from(arg);
TemporalHelpers.assertPlainYearMonth(result, 2019, 6, "M06", `Calendar created from string "${calendar}"`);
