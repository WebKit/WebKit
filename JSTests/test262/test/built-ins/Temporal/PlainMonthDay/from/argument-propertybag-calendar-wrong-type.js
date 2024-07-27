// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: >
  Appropriate error thrown when a calendar property from a property bag cannot
  be converted to a calendar ID
features: [BigInt, Symbol, Temporal]
---*/

const primitiveTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
];

for (const [calendar, description] of primitiveTests) {
  const arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(
    typeof calendar === 'string' ? RangeError : TypeError,
    () => Temporal.PlainMonthDay.from(arg),
    `${description} does not convert to a valid ISO string`
  );
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "object"],
  [new Temporal.Duration(), "duration instance"],
];

for (const [calendar, description] of typeErrorTests) {
  const arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(TypeError, () => Temporal.PlainMonthDay.from(arg), `${description} is not a valid property bag and does not convert to a string`);
}
