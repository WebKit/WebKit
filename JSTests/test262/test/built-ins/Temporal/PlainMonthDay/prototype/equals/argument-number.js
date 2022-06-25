// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: A number is converted to a string, then to Temporal.PlainMonthDay
features: [Temporal]
---*/

const instance = new Temporal.PlainMonthDay(11, 18);

const arg = 1118;

const result = instance.equals(arg);
assert.sameValue(result, true, "1118 is a valid ISO string for PlainMonthDay");

const numbers = [
  1,
  -1118,
  12345,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.equals(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainMonthDay`
  );
}
