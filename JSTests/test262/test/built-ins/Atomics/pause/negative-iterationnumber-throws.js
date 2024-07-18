// Copyright 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.pause
description: Atomics.pause throws on negative argument values
features: [Atomics.pause]
---*/

const values = [
  -1,
  Number.MIN_SAFE_INTEGER,
  Number.MIN_SAFE_INTEGER - 1
];

for (const v of values) {
  assert.throws(RangeError, () => { Atomics.pause(v); },
                `${v} is an illegal iterationNumber`);
}
