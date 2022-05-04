// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: A number is converted to a string, then to Temporal.PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const arg = 19761118;

const result = Temporal.PlainDate.from(arg);
TemporalHelpers.assertPlainDate(result, 1976, 11, "M11", 18, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainDate.from(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}
