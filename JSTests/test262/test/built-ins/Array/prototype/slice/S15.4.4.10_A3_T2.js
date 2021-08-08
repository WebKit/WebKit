// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
esid: sec-array.prototype.slice
description: length = 4294967297
---*/

var obj = {};
obj.slice = Array.prototype.slice;
obj[0] = "x";
obj[4294967296] = "y";
obj.length = 4294967297;

try {
  var arr = obj.slice(0, 4294967297);
  throw new Test262Error('#1: var obj = {}; obj.slice = Array.prototype.slice; obj[0] = "x"; obj[4294967296] = "y"; obj.length = 4294967297; var arr = obj.slice(0,4294967297); lead to throwing exception.');
} catch (e) {
  if (!(e instanceof RangeError)) {
    throw new Test262Error('#1.1: var obj = {}; obj.slice = Array.prototype.slice; obj[0] = "x"; obj[4294967296] = "y"; obj.length = 4294967297; var arr = obj.slice(0,4294967297); lead to throwing exception. Exception is instance of RangeError. Actual: exception is ' + e);
  }
}
