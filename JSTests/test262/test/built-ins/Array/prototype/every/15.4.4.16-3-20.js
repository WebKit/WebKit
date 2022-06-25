// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.every
description: >
    Array.prototype.every - value of 'length' is an Object which has
    an own valueOf method
---*/

function callbackfn1(val, idx, obj) {
  return val > 10;
}

function callbackfn2(val, idx, obj) {
  return val > 11;
}

var valueOfAccessed = false;

var obj = {
  0: 12,
  1: 11,
  2: 9,
  length: {
    valueOf: function() {
      valueOfAccessed = true;
      return 2;
    }
  }
};

assert(Array.prototype.every.call(obj, callbackfn1), 'Array.prototype.every.call(obj, callbackfn1) !== true');
assert.sameValue(Array.prototype.every.call(obj, callbackfn2), false, 'Array.prototype.every.call(obj, callbackfn2)');
assert(valueOfAccessed, 'valueOfAccessed !== true');
