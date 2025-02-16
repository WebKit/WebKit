// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
  Iterator.from returns an iterator wrapper if O is not an instance of Iterator.

  Iterator is not enabled unconditionally
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - iterator-helpers
description: |
  pending
esid: pending
---*/
class TestIterator {
  [Symbol.iterator]() {
    return this;
  }

  next() {
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assert.sameValue(iter instanceof Iterator, false);

const wrapper = Iterator.from(iter);
assert.sameValue(iter !== wrapper, true);
assert.sameValue(wrapper instanceof Iterator, true);

