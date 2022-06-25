// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Leap second is a valid ISO string for a calendar in a property bag
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = "2016-12-31T23:59:60";

let arg = { monthCode: "M11", day: 18, calendar };
const result1 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(
  result1,
  "M11", 18,
  "leap second is a valid ISO string for calendar"
);

arg = { monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(
  result2,
  "M11", 18,
  "leap second is a valid ISO string for calendar (nested property)"
);
