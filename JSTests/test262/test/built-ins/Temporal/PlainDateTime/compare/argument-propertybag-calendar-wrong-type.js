// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
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
      typeof calendar === "string" ? RangeError : TypeError,
      () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)),
      `${description} does not convert to a valid ISO string (first argument)`
    );
  assert.throws(
    typeof calendar === "string" ? RangeError : TypeError,
    () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg),
    `${description} does not convert to a valid ISO string (second argument)`
  );
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "object"],
  [new Temporal.Duration(), "duration instance"],
];

for (const [calendar, description] of typeErrorTests) {
  const arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `${description} is not a valid property bag and does not convert to a string (first argument)`);
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `${description} is not a valid property bag and does not convert to a string (second argument)`);
}
