// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: A number is converted to a string, then to Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC");

const arg = 19761118;

const result = instance.withPlainDate(arg);
assert.sameValue(result.epochNanoseconds, 217_129_600_000_000_000n, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.withPlainDate(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}
