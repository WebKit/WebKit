// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: Leap second is a valid ISO string for Calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, { id: "replace-me" });

let arg = "2016-12-31T23:59:60";
const result1 = instance.withCalendar(arg);
assert.sameValue(
  result1.calendar.id,
  "iso8601",
  "leap second is a valid ISO string for Calendar"
);

arg = { calendar: "2016-12-31T23:59:60" };
const result2 = instance.withCalendar(arg);
assert.sameValue(
  result2.calendar.id,
  "iso8601",
  "leap second is a valid ISO string for Calendar (nested property)"
);
