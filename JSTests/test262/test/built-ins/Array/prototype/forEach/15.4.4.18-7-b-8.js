// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.foreach
description: >
    Array.prototype.forEach - deleting own property causes index
    property not to be visited on an Array-like object
---*/

var accessed = false;
var testResult = true;

function callbackfn(val, idx, obj) {
  accessed = true;
  if (idx === 1) {
    testResult = false;
  }
}

var obj = {
  length: 2
};

Object.defineProperty(obj, "1", {
  get: function() {
    return 6.99;
  },
  configurable: true
});

Object.defineProperty(obj, "0", {
  get: function() {
    delete obj[1];
    return 0;
  },
  configurable: true
});

Array.prototype.forEach.call(obj, callbackfn);

assert(testResult, 'testResult !== true');
assert(accessed, 'accessed !== true');
