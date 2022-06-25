// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.lastindexof
description: >
    Array.prototype.lastIndexOf -  deleting property of prototype
    causes prototype index property not to be visited on an Array-like
    Object
---*/

var arr = {
  2: 2,
  length: 20
};

Object.defineProperty(arr, "3", {
  get: function() {
    delete Object.prototype[1];
    return 0;
  },
  configurable: true
});

Object.prototype[1] = 1;

assert.sameValue(Array.prototype.lastIndexOf.call(arr, 1), -1, 'Array.prototype.lastIndexOf.call(arr, 1)');
