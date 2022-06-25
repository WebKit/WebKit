// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.foreach
description: >
    Array.prototype.forEach - element changed by getter on previous
    iterations is observed on an Array-like object
---*/

var preIterVisible = false;
var obj = {
  length: 2
};
var testResult = false;

function callbackfn(val, idx, obj) {
  if (idx === 1) {
    testResult = (val === 9);
  }
}

Object.defineProperty(obj, "0", {
  get: function() {
    preIterVisible = true;
    return 11;
  },
  configurable: true
});

Object.defineProperty(obj, "1", {
  get: function() {
    if (preIterVisible) {
      return 9;
    } else {
      return 13;
    }
  },
  configurable: true
});

Array.prototype.forEach.call(obj, callbackfn);

assert(testResult, 'testResult !== true');
