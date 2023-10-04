// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: A number as calendar in relativeTo property bag is invalid
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(
    TypeError,
    () => instance.total({ unit: "days", relativeTo }),
    `A number (${calendar}) is not a valid ISO string for relativeTo.calendar`
  );
}
