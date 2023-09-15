// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: A number cannot be used in place of a Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const numbers = [
  1,
  19761118,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    TypeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 18)),
    "A number is not a valid ISO string for PlainDate (first argument)"
  );
  assert.throws(
    TypeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 18), arg),
    "A number is not a valid ISO string for PlainDate (second argument)"
  );
}
