// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Strict equality comparison of BigInt and miscellaneous primitive values
esid: sec-strict-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.

features: [BigInt, Symbol]
---*/

assert.sameValue(0n === undefined, false, "0n === undefined");
assert.sameValue(undefined === 0n, false, "undefined === 0n");
assert.sameValue(1n === undefined, false, "1n === undefined");
assert.sameValue(undefined === 1n, false, "undefined === 1n");
assert.sameValue(0n === null, false, "0n === null");
assert.sameValue(null === 0n, false, "null === 0n");
assert.sameValue(1n === null, false, "1n === null");
assert.sameValue(null === 1n, false, "null === 1n");
assert.sameValue(0n === Symbol("1"), false, '0n === Symbol("1")');
assert.sameValue(Symbol("1") === 0n, false, 'Symbol("1") === 0n');
assert.sameValue(1n === Symbol("1"), false, '1n === Symbol("1")');
assert.sameValue(Symbol("1") === 1n, false, 'Symbol("1") === 1n');
