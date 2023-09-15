// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: A number cannot be used in place of a Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const numbers = [
  1,
  19761118,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    TypeError,
    () => instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" }),
    'Numbers cannot be used in place of an ISO string for PlainDate'
  );
}
