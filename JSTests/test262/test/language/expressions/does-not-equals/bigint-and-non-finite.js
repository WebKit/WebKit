// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Non-strict inequality comparison of BigInt and non-finite Number values
esid: sec-abstract-equality-comparison
info: |
  12. If Type(x) is BigInt and Type(y) is Number, or if Type(x) is Number and Type(y) is BigInt,
    a. If x or y are any of NaN, +∞, or -∞, return false.

features: [BigInt]
---*/

assert.sameValue(0n != Infinity, true, "0n != Infinity");
assert.sameValue(Infinity != 0n, true, "Infinity != 0n");
assert.sameValue(1n != Infinity, true, "1n != Infinity");
assert.sameValue(Infinity != 1n, true, "Infinity != 1n");
assert.sameValue(-1n != Infinity, true, "-1n != Infinity");
assert.sameValue(Infinity != -1n, true, "Infinity != -1n");
assert.sameValue(0n != -Infinity, true, "0n != -Infinity");
assert.sameValue(-Infinity != 0n, true, "-Infinity != 0n");
assert.sameValue(1n != -Infinity, true, "1n != -Infinity");
assert.sameValue(-Infinity != 1n, true, "-Infinity != 1n");
assert.sameValue(-1n != -Infinity, true, "-1n != -Infinity");
assert.sameValue(-Infinity != -1n, true, "-Infinity != -1n");
assert.sameValue(0n != NaN, true, "0n != NaN");
assert.sameValue(NaN != 0n, true, "NaN != 0n");
assert.sameValue(1n != NaN, true, "1n != NaN");
assert.sameValue(NaN != 1n, true, "NaN != 1n");
assert.sameValue(-1n != NaN, true, "-1n != NaN");
assert.sameValue(NaN != -1n, true, "NaN != -1n");
