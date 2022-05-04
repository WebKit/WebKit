// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: A number is converted to a string, then to Temporal.PlainDate
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const arg = 19761118;

const result = instance.toPlainDateTime(arg);
TemporalHelpers.assertPlainDateTime(result, 1976, 11, "M11", 18, 12, 34, 56, 987, 654, 321, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.toPlainDateTime(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}
