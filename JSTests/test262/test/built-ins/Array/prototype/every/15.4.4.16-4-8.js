// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.every
es5id: 15.4.4.16-4-8
description: >
    Array.prototype.every - side effects produced by step 2 are
    visible when an exception occurs
---*/

var obj = {
  0: 11,
  1: 12
};

var accessed = false;

Object.defineProperty(obj, "length", {
  get: function() {
    accessed = true;
    return 2;
  },
  configurable: true
});
assert.throws(TypeError, function() {
  Array.prototype.every.call(obj, null);
});
assert(accessed, 'accessed !== true');
