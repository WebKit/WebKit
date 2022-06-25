// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: A number is converted to a string, then to Temporal.PlainDateTime
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let arg = 19761118;

const result = Temporal.PlainDateTime.from(arg);
TemporalHelpers.assertPlainDateTime(result, 1976, 11, "M11", 18, 0, 0, 0, 0, 0, 0, "19761118 is a valid ISO string for PlainDateTime");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainDateTime.from(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDateTime`
  );
}
