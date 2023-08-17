// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: A number as relativeTo option is converted to a string, then to Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

const relativeTo = 20191101;

const numbers = [
  1,
  20191101,
  -20191101,
  1234567890,
];

for (const relativeTo of numbers) {
  assert.throws(
    TypeError,
    () => instance.round({ largestUnit: "years", relativeTo }),
    `Number ${relativeTo} does not convert to a valid ISO string for relativeTo`
  );
}
