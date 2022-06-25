// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.until
description: TypeError thrown when options argument is a primitive
features: [BigInt, Symbol, Temporal]
---*/

const badOptions = [
  null,
  true,
  "some string",
  Symbol(),
  1,
  2n,
];

const instance = new Temporal.Instant(0n);
for (const value of badOptions) {
  assert.throws(TypeError, () => instance.until(new Temporal.Instant(3600_000_000_000n), value),
    `TypeError on wrong options type ${typeof value}`);
};
