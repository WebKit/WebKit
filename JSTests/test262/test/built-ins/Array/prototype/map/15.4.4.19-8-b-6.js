// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.map
description: >
    Array.prototype.map - properties can be added to prototype after
    current position are visited on an Array-like object
---*/

function callbackfn(val, idx, obj) {
  if (idx === 1 && val === 6.99) {
    return false;
  } else {
    return true;
  }
}
var obj = {
  length: 2
};

Object.defineProperty(obj, "0", {
  get: function() {
    Object.defineProperty(Object.prototype, "1", {
      get: function() {
        return 6.99;
      },
      configurable: true
    });
    return 0;
  },
  configurable: true
});

var testResult = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
assert.sameValue(testResult[1], false, 'testResult[1]');
