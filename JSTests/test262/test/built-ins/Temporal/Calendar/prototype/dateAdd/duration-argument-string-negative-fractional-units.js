// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Strings with fractional duration units are treated with the correct sign
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const instance = new Temporal.PlainDate(2000, 5, 2, calendar);

const resultHours = calendar.dateAdd(instance, "-PT24.567890123H");
TemporalHelpers.assertPlainDate(resultHours, 2000, 5, "M05", 1, "negative fractional hours");

const resultMinutes = calendar.dateAdd(instance, "-PT1440.567890123M");
TemporalHelpers.assertPlainDate(resultMinutes, 2000, 5, "M05", 1, "negative fractional minutes");
