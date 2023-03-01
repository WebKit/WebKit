// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: The calendar name is case-insensitive
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = "IsO8601";

let arg = { monthCode: "M11", day: 18, calendar };
const result1 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(result1, "M11", 18, "Calendar is case-insensitive");

arg = { monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(result2, "M11", 18, "Calendar is case-insensitive (nested property)");
