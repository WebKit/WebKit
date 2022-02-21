// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Strings with fractional duration units are treated with the correct sign
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2000, 5);

const resultHours = instance.add("-PT24.567890123H");
TemporalHelpers.assertPlainYearMonth(resultHours, 2000, 5, "M05", "negative fractional hours");

const resultMinutes = instance.add("-PT1440.567890123M");
TemporalHelpers.assertPlainYearMonth(resultMinutes, 2000, 5, "M05", "negative fractional minutes");
