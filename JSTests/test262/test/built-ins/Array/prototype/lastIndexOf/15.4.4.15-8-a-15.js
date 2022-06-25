// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.lastindexof
description: >
    Array.prototype.lastIndexOf -  deleting own property with
    prototype property causes prototype index property to be visited
    on an Array-like object
---*/

var arr = {
  0: 0,
  1: 111,
  2: 2,
  length: 10
};

Object.defineProperty(arr, "6", {
  get: function() {
    delete arr[1];
    return 0;
  },
  configurable: true
});

Object.prototype[1] = 1;

assert.sameValue(Array.prototype.lastIndexOf.call(arr, 1), 1, 'Array.prototype.lastIndexOf.call(arr, 1)');
