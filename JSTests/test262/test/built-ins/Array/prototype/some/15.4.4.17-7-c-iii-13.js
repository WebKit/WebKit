// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: >
    Array.prototype.some - return value of callbackfn is an empty
    string
---*/

var accessed = false;

function callbackfn(val, idx, obj) {
  accessed = true;
  return "";
}

assert.sameValue([11].some(callbackfn), false, '[11].some(callbackfn)');
assert(accessed, 'accessed !== true');
