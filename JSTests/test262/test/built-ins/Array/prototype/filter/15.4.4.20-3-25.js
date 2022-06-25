// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.filter
description: >
    Array.prototype.filter - value of 'length' is a negative
    non-integer
---*/

function callbackfn(val, idx, obj) {
  return true;
}

var obj = {
  1: 11,
  2: 9,
  length: -4294967294.5
};

var newArr = Array.prototype.filter.call(obj, callbackfn);

assert.sameValue(newArr.length, 0, 'newArr.length');
assert.sameValue(newArr[0], undefined, 'newArr[0]');
