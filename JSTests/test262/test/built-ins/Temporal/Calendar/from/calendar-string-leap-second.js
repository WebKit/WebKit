// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: Leap second is a valid ISO string for Calendar
features: [Temporal]
---*/

const arg = "2016-12-31T23:59:60";
const result = Temporal.Calendar.from(arg);
assert.sameValue(
  result.id,
  "iso8601",
  "leap second is a valid ISO string for Calendar"
);
