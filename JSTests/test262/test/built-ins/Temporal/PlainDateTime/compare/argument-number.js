// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: A number is converted to a string, then to Temporal.PlainDateTime
features: [Temporal]
---*/

let arg = 19761118;

const result1 = Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18));
assert.sameValue(result1, 0, "19761118 is a valid ISO string for PlainDateTime (first argument)");
const result2 = Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg);
assert.sameValue(result2, 0, "19761118 is a valid ISO string for PlainDateTime (second argument)");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)),
    `Number ${arg} does not convert to a valid ISO string for PlainDateTime (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDateTime (second argument)`
  );
}
