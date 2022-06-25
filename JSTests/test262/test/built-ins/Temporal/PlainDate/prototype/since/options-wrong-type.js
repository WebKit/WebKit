// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
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

const instance = new Temporal.PlainDate(2000, 5, 2);
for (const value of badOptions) {
  assert.throws(TypeError, () => instance.since(new Temporal.PlainDate(1976, 11, 18), value),
    `TypeError on wrong options type ${typeof value}`);
};
