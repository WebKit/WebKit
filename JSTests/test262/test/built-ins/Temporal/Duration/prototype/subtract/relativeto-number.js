// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: A number as relativeTo option is converted to a string, then to Temporal.PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 1);

const relativeTo = 20191101;

const result = instance.subtract(new Temporal.Duration(0, 0, 0, 0, 24), { relativeTo });
TemporalHelpers.assertDuration(result, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "20191101 is a valid ISO string for relativeTo");

const numbers = [
  1,
  -20191101,
  1234567890,
];

for (const relativeTo of numbers) {
  assert.throws(
    RangeError,
    () => instance.subtract(new Temporal.Duration(0, 0, 0, 0, 24), { relativeTo }),
    `Number ${relativeTo} does not convert to a valid ISO string for relativeTo`
  );
}
