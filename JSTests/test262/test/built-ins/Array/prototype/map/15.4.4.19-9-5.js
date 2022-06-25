// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.map
description: >
    Array.prototype.map - empty array to be returned if 'length' is 0
    (empty array)
---*/

function callbackfn(val, idx, obj) {
  return val > 10;
}

var obj = {
  0: 9,
  1: 8,
  length: 0
};

var testResult = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(testResult.length, 0, 'testResult.length');
