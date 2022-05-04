// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: Do not show calendar if calendar name option is "never"
features: [Temporal]
---*/

const dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23);
const cal = {
  toString() { return "bogus"; }
};
const expected = "1976-11-18T15:23:00";

assert.sameValue(
  dt.toString({ calendarName: "never" }),
  expected,
  "Do not show calendar if calendarName = never"
);

assert.sameValue(
  dt.withCalendar(cal).toString({ calendarName: "never" }),
  expected,
  "Do not show calendar when calendarName = never, even if non-ISO calendar is used"
);
