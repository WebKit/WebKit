// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.erayear
description: A number is converted to a string, then to Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const arg = 19761118;

const result = instance.eraYear(arg);
assert.sameValue(result, undefined, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.eraYear(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}
