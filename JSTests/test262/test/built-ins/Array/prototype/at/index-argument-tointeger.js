// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.at
description: >
  Property type and descriptor.
info: |
  Array.prototype.at( index )

  Let relativeIndex be ? ToInteger(index).

features: [Array.prototype.at]
---*/
assert.sameValue(
  typeof Array.prototype.at,
  'function',
  'The value of `typeof Array.prototype.at` is expected to be "function"'
);

let valueOfCallCount = 0;
let index = {
  valueOf() {
    valueOfCallCount++;
    return 1;
  }
};

let a = [0,1,2,3];

assert.sameValue(a.at(index), 1, 'a.at({valueOf() {valueOfCallCount++; return 1;}}) must return 1');
assert.sameValue(valueOfCallCount, 1, 'The value of valueOfCallCount is expected to be 1');
