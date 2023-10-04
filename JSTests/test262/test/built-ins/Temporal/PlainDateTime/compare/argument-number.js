// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: A number cannot be used in place of a Temporal.PlainDateTime
features: [Temporal]
---*/

const numbers = [
  1,
  19761118,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    TypeError,
    () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)),
    `A number (${arg}) is not a valid ISO string for PlainDateTime (first argument)`
  );
  assert.throws(
    TypeError,
    () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg),
    `A number (${arg}) is not a valid ISO string for PlainDateTime (second argument)`
  );
}
