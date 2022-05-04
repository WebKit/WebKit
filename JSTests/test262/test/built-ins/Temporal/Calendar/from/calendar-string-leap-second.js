// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: Leap second is a valid ISO string for Calendar
features: [Temporal]
---*/

let arg = "2016-12-31T23:59:60";
const result1 = Temporal.Calendar.from(arg);
assert.sameValue(
  result1.id,
  "iso8601",
  "leap second is a valid ISO string for Calendar"
);

arg = { calendar: "2016-12-31T23:59:60" };
const result2 = Temporal.Calendar.from(arg);
assert.sameValue(
  result2.id,
  "iso8601",
  "leap second is a valid ISO string for Calendar (nested property)"
);
