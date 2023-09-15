// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: A number as calendar in a property bag is not accepted
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  assert.throws(
    TypeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 19)),
    "A number is not a valid ISO string for calendar (first argument)"
  );
  assert.throws(
    TypeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 19), arg),
    "A number is not a valid ISO string for calendar (second argument)"
  );
}
