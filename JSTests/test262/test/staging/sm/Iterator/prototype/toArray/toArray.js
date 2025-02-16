// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - iterator-helpers
info: |
  Iterator is not enabled unconditionally
description: |
  pending
esid: pending
---*/

const iter = [1, 2, 3].values();
assert.sameValue(Array.isArray(iter), false);

const array = iter.toArray();
assert.sameValue(Array.isArray(array), true);
assert.sameValue(array.length, 3);

const expected = [1, 2, 3];
for (const item of array) {
  const expect = expected.shift();
  assert.sameValue(item, expect);
}

