// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: A number is converted to a string, then to Temporal.PlainDate
features: [Temporal]
---*/

const arg = 19761118;

const result1 = Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18));
assert.sameValue(result1, 0, "19761118 is a valid ISO string for PlainDate (first argument)");
const result2 = Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg);
assert.sameValue(result2, 0, "19761118 is a valid ISO string for PlainDate (second argument)");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18)),
    `Number ${arg} does not convert to a valid ISO string for PlainDate (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate (second argument)`
  );
}
