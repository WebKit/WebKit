// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Strict equality comparison of BigInt and non-finite Number values
esid: sec-strict-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.

features: [BigInt]
---*/

assert.sameValue(0n === Infinity, false, "0n === Infinity");
assert.sameValue(Infinity === 0n, false, "Infinity === 0n");
assert.sameValue(1n === Infinity, false, "1n === Infinity");
assert.sameValue(Infinity === 1n, false, "Infinity === 1n");
assert.sameValue(-1n === Infinity, false, "-1n === Infinity");
assert.sameValue(Infinity === -1n, false, "Infinity === -1n");
assert.sameValue(0n === -Infinity, false, "0n === -Infinity");
assert.sameValue(-Infinity === 0n, false, "-Infinity === 0n");
assert.sameValue(1n === -Infinity, false, "1n === -Infinity");
assert.sameValue(-Infinity === 1n, false, "-Infinity === 1n");
assert.sameValue(-1n === -Infinity, false, "-1n === -Infinity");
assert.sameValue(-Infinity === -1n, false, "-Infinity === -1n");
assert.sameValue(0n === NaN, false, "0n === NaN");
assert.sameValue(NaN === 0n, false, "NaN === 0n");
assert.sameValue(1n === NaN, false, "1n === NaN");
assert.sameValue(NaN === 1n, false, "NaN === 1n");
assert.sameValue(-1n === NaN, false, "-1n === NaN");
assert.sameValue(NaN === -1n, false, "NaN === -1n");
