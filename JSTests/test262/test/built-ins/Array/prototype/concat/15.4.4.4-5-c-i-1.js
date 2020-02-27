// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.concat
description: >
    Array.prototype.concat will concat an Array when index property
    (read-only) exists in Array.prototype (Step 5.c.i)
includes: [propertyHelper.js]
---*/

Object.defineProperty(Array.prototype, "0", {
  value: 100,
  writable: false,
  configurable: true
});

var newArr = Array.prototype.concat.call(101);

assert(newArr[0] instanceof Number);
verifyProperty(newArr, "0", {
  writable: true,
  enumerable: true,
  configurable: true,
});
