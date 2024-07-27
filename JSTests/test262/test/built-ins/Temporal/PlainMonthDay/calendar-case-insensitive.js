// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: Calendar names are case-insensitive
features: [Temporal]
---*/

let arg = "iSo8601";

const result = new Temporal.PlainMonthDay(12, 15, arg, 1972);
assert.sameValue(result.calendarId, "iso8601", "Calendar is case-insensitive");

arg = "\u0130SO8601";
assert.throws(
  RangeError,
  () => new Temporal.PlainMonthDay(12, 15, arg, 1972),
  "calendar ID is capital dotted I is not lowercased"
);
