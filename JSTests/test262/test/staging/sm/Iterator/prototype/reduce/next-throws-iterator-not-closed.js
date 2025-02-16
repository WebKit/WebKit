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

class TestIterator extends Iterator {
  next() {
    throw new Error();
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const sum = (x, y) => x + y;
const iter = new TestIterator();

assert.sameValue(iter.closed, false);
assertThrowsInstanceOf(() => iter.reduce(sum), Error);
assert.sameValue(iter.closed, false);

