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
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assertThrowsInstanceOf(() => iter.reduce(), TypeError);
assertThrowsInstanceOf(() => iter.reduce(undefined), TypeError);
assertThrowsInstanceOf(() => iter.reduce(null), TypeError);
assertThrowsInstanceOf(() => iter.reduce(0), TypeError);
assertThrowsInstanceOf(() => iter.reduce(false), TypeError);
assertThrowsInstanceOf(() => iter.reduce(''), TypeError);
assertThrowsInstanceOf(() => iter.reduce(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.reduce({}), TypeError);

