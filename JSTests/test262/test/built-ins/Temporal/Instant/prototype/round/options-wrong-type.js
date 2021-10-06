// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.round
description: TypeError thrown when options argument is missing or a primitive
features: [Symbol, Temporal]
---*/

const values = [
  undefined,
  null,
  true,
  "string",
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.Instant(0n);
assert.throws(TypeError, () => instance.round(), "missing argument");
for (const value of values) {
  assert.throws(TypeError, () => instance.round(value), `argument ${String(value)}`);
}
