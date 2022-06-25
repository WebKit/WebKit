// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: >
    Array.prototype.indexOf - deleting own property causes index
    property not to be visited on an Array
---*/

var arr = [1, 2];

Object.defineProperty(arr, "1", {
  get: function() {
    return "6.99";
  },
  configurable: true
});

Object.defineProperty(arr, "0", {
  get: function() {
    delete arr[1];
    return 0;
  },
  configurable: true
});

assert.sameValue(arr.indexOf("6.99"), -1, 'arr.indexOf("6.99")');
