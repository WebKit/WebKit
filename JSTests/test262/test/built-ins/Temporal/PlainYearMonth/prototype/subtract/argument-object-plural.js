// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Singular properties are ignored
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const ym = Temporal.PlainYearMonth.from("2019-11");
TemporalHelpers.assertPlainYearMonth(ym.subtract({ month: 1, years: 1 }), 2018, 11, "M11");
