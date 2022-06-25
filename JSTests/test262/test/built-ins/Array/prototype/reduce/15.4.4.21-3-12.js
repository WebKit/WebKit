// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduce
description: >
    Array.prototype.reduce - 'length' is a string containing a
    negative number
---*/

function callbackfn(prevVal, curVal, idx, obj) {
  return (curVal === 11 && idx === 1);
}

var obj = {
  1: 11,
  2: 9,
  length: "-4294967294"
};

assert.sameValue(Array.prototype.reduce.call(obj, callbackfn, 1), 1, 'Array.prototype.reduce.call(obj, callbackfn, 1)');
