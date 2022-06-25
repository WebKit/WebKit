// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: Calendar.from should support iso8601.
features: [Temporal]
---*/

const tests = [
  "iso8601",
  "1994-11-05T08:15:30-05:00",
];

for (const item of tests) {
  const calendar = Temporal.Calendar.from(item);
  assert(calendar instanceof Temporal.Calendar);
  assert.sameValue(calendar.id, "iso8601");
}
