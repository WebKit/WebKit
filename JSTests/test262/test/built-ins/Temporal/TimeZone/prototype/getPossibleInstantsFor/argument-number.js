// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: A number is converted to a string, then to Temporal.PlainDateTime
includes: [compareArray.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

let arg = 19761118;

const result = instance.getPossibleInstantsFor(arg);
assert.compareArray(result.map(i => i.epochNanoseconds), [217_123_200_000_000_000n], "19761118 is a valid ISO string for PlainDateTime");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.getPossibleInstantsFor(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDateTime`
  );
}
