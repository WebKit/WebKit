// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: A number is invalid in place of an ISO string for Temporal.PlainTime
features: [Temporal]
---*/

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    TypeError,
    () => Temporal.PlainTime.from(arg),
    "A number is not a valid ISO string for PlainTime"
  );
}
