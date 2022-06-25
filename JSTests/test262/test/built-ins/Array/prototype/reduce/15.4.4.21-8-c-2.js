// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduce
description: >
    Array.prototype.reduce throws TypeError when elements assigned
    values are deleted by reducing array length and initialValue is
    not present
---*/

function callbackfn(prevVal, curVal, idx, obj)
{}

var arr = new Array(10);
arr[9] = 1;
arr.length = 5;
assert.throws(TypeError, function() {
  arr.reduce(callbackfn);
});
