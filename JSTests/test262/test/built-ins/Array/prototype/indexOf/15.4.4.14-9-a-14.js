// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.indexof
description: >
    Array.prototype.indexOf - deleting property of prototype causes
    prototype index property not to be visited on an Array
---*/

var arr = [0, , 2];

Object.defineProperty(arr, "0", {
  get: function() {
    delete Array.prototype[1];
    return 0;
  },
  configurable: true
});

Array.prototype[1] = 1;

assert.sameValue(arr.indexOf(1), -1, 'arr.indexOf(1)');
