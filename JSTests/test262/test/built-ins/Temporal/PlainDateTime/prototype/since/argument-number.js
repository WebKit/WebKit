// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: A number is converted to a string, then to Temporal.PlainDateTime
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(1976, 11, 18);

let arg = 19761118;

const result = instance.since(arg);
TemporalHelpers.assertDuration(result, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "19761118 is a valid ISO string for PlainDateTime");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.since(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDateTime`
  );
}
