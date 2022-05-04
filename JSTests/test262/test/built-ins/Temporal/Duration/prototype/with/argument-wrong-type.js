// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: Throw TypeError if the temporalDurationLike argument is the wrong type
features: [Temporal]
---*/

let d = new Temporal.Duration(1, 2, 3, 4, 5);

[
  "string",
  "P1YT1M",
  true,
  false,
  NaN,
  Infinity,
  undefined,
  null,
  123,
  Symbol(),
  456n,
].forEach((badInput) => {
  assert.throws(TypeError, () => d.with(badInput),
    "Throw TypeError if temporalDurationLike is not valid");
});
