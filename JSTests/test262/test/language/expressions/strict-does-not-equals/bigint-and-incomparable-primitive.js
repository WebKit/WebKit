// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Strict inequality comparison of BigInt and miscellaneous primitive values
esid: sec-strict-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.

features: [BigInt, Symbol]
---*/

assert.sameValue(0n !== undefined, true, "0n !== undefined");
assert.sameValue(undefined !== 0n, true, "undefined !== 0n");
assert.sameValue(1n !== undefined, true, "1n !== undefined");
assert.sameValue(undefined !== 1n, true, "undefined !== 1n");
assert.sameValue(0n !== null, true, "0n !== null");
assert.sameValue(null !== 0n, true, "null !== 0n");
assert.sameValue(1n !== null, true, "1n !== null");
assert.sameValue(null !== 1n, true, "null !== 1n");
assert.sameValue(0n !== Symbol("1"), true, '0n !== Symbol("1")');
assert.sameValue(Symbol("1") !== 0n, true, 'Symbol("1") !== 0n');
assert.sameValue(1n !== Symbol("1"), true, '1n !== Symbol("1")');
assert.sameValue(Symbol("1") !== 1n, true, 'Symbol("1") !== 1n');
