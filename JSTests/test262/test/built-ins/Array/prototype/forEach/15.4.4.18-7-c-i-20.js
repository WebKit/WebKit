// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.foreach
description: >
    Array.prototype.forEach - element to be retrieved is own accessor
    property without a get function that overrides an inherited
    accessor property on an Array
---*/

var testResult = false;

function callbackfn(val, idx, obj) {
  if (idx === 0) {
    testResult = (typeof val === "undefined");
  }
}

var arr = [];

Object.defineProperty(arr, "0", {
  set: function() {},
  configurable: true
});

Object.defineProperty(Array.prototype, "0", {
  get: function() {
    return 100;
  },
  configurable: true
});

arr.forEach(callbackfn);

assert(testResult, 'testResult !== true');
