// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: A number cannot be used in place of a relativeTo
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 1);

const numbers = [
  1,
  -20191101,
  20191101,
  1234567890,
];

for (const relativeTo of numbers) {
  assert.throws(
    TypeError,
    () => instance.add(new Temporal.Duration(0, 0, 0, 0, -24), { relativeTo }),
    `A number (${relativeTo}) is not a valid ISO string for relativeTo`
  );
}
