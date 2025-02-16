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

class TestError extends Error {}
class TestIterator extends Iterator {
  next() {
    return new Proxy({done: false}, {get: (target, key, receiver) => {
      if (key === 'value')
        throw new TestError();
      return 0;
    }});
  }

  closed = false;
  return() {
    closed = true;
  }
}

const iterator = new TestIterator();
assert.sameValue(iterator.closed, false, 'iterator starts unclosed');
assertThrowsInstanceOf(() => iterator.some(x => x), TestError);
assert.sameValue(iterator.closed, false, 'iterator remains unclosed');

