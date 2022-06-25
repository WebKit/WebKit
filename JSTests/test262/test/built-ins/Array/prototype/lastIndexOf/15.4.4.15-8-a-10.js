// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.lastindexof
description: >
    Array.prototype.lastIndexOf - properties can be added to prototype
    after current position are visited on an Array
---*/

var arr = [0, , 2];

Object.defineProperty(arr, "2", {
  get: function() {
    Object.defineProperty(Array.prototype, "1", {
      get: function() {
        return 6.99;
      },
      configurable: true
    });
    return 0;
  },
  configurable: true
});

assert.sameValue(arr.lastIndexOf(6.99), 1, 'arr.lastIndexOf(6.99)');
