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
  constructor(value) {
    super();
    this.value = value;
  }

  next() {
    return this.value;
  }
}

const sum = (x, y) => x + y;

let iter = new TestIterator(undefined);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(null);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(0);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(false);
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator('');
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);
iter = new TestIterator(Symbol(''));
assertThrowsInstanceOf(() => iter.reduce(sum), TypeError);

