// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Strict inequality comparison of BigInt and Boolean values
esid: sec-strict-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.

features: [BigInt]
---*/

assert.sameValue(-1n !== false, true, "-1n !== false");
assert.sameValue(false !== -1n, true, "false !== -1n");
assert.sameValue(-1n !== true, true, "-1n !== true");
assert.sameValue(true !== -1n, true, "true !== -1n");
assert.sameValue(0n !== false, true, "0n !== false");
assert.sameValue(false !== 0n, true, "false !== 0n");
assert.sameValue(0n !== true, true, "0n !== true");
assert.sameValue(true !== 0n, true, "true !== 0n");
assert.sameValue(1n !== false, true, "1n !== false");
assert.sameValue(false !== 1n, true, "false !== 1n");
assert.sameValue(1n !== true, true, "1n !== true");
assert.sameValue(true !== 1n, true, "true !== 1n");
assert.sameValue(2n !== false, true, "2n !== false");
assert.sameValue(false !== 2n, true, "false !== 2n");
assert.sameValue(2n !== true, true, "2n !== true");
assert.sameValue(true !== 2n, true, "true !== 2n");
