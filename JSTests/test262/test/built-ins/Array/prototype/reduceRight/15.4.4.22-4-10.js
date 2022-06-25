// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduceright
description: >
    Array.prototype.reduceRight - the exception is not thrown if
    exception was thrown by step 2
---*/

var obj = {
  0: 11,
  1: 12
};

Object.defineProperty(obj, "length", {
  get: function() {
    throw new Test262Error();
  },
  configurable: true
});

assert.throws(Test262Error, function() {
  Array.prototype.reduceRight.call(obj, undefined);
});
