// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: Array.prototype.some - return value of callbackfn is undefined
---*/

var accessed = false;

function callbackfn(val, idx, obj) {
  accessed = true;
  return undefined;
}

var obj = {
  0: 11,
  length: 2
};

assert.sameValue(Array.prototype.some.call(obj, callbackfn), false, 'Array.prototype.some.call(obj, callbackfn)');
assert(accessed, 'accessed !== true');
