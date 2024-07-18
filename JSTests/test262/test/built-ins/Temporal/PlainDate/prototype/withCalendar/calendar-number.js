// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: A number is not allowed to be a calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(1976, 11, 18, "iso8601");

const numbers = [
  1,
  -19761118,
  19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    TypeError,
    () => instance.withCalendar(arg),
    "A number is not a valid ISO string for Calendar"
  );
}
