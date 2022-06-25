// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.every
description: >
    Array.prototype.every - k values are passed in ascending numeric
    order
---*/

var arr = [0, 1, 2, 3, 4, 5];
var lastIdx = 0;
var called = 0;

function callbackfn(val, idx, o) {
  called++;
  if (lastIdx !== idx) {
    return false;
  } else {
    lastIdx++;
    return true;
  }
}

assert(arr.every(callbackfn), 'arr.every(callbackfn) !== true');
assert.sameValue(arr.length, called, 'arr.length');
