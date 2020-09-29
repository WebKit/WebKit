// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Property type and descriptor.
info: |
  Array.prototype.item( index )

  Let relativeIndex be ? ToInteger(index).

features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

let valueOfCallCount = 0;
let index = {
  valueOf() {
    valueOfCallCount++;
    return 1;
  }
};

let a = [0,1,2,3];

assert.sameValue(a.item(index), 1, 'a.item({valueOf() {valueOfCallCount++; return 1;}}) must return 1');
assert.sameValue(valueOfCallCount, 1, 'The value of `valueOfCallCount` is 1');
