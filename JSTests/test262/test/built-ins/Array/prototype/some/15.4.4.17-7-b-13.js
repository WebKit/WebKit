// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: >
    Array.prototype.some - deleting own property with prototype
    property causes prototype index property to be visited on an Array
---*/

function callbackfn(val, idx, obj) {
  if (idx === 1 && val === 1) {
    return true;
  } else {
    return false;
  }
}
var arr = [0, 111, 2];

Object.defineProperty(arr, "0", {
  get: function() {
    delete arr[1];
    return 0;
  },
  configurable: true
});

Array.prototype[1] = 1;

assert(arr.some(callbackfn), 'arr.some(callbackfn) !== true');
