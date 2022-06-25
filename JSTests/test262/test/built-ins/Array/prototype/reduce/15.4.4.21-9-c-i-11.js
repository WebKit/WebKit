// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduce
description: >
    Array.prototype.reduce - element to be retrieved is own accessor
    property that overrides an inherited data property on an
    Array-like object
---*/

var testResult = false;
var initialValue = 0;

function callbackfn(prevVal, curVal, idx, obj) {
  if (idx === 1) {
    testResult = (curVal === "11");
  }
}

var proto = {
  0: 0,
  1: 1,
  2: 2
};

var Con = function() {};
Con.prototype = proto;

var child = new Con();
child.length = 3;

Object.defineProperty(child, "1", {
  get: function() {
    return "11";
  },
  configurable: true
});

Array.prototype.reduce.call(child, callbackfn, initialValue);

assert(testResult, 'testResult !== true');
