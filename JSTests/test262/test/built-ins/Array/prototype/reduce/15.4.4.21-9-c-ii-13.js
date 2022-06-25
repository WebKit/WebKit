// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduce
description: >
    Array.prototype.reduce - callbackfn is called with 4 formal
    parameter
---*/

var result = false;

function callbackfn(prevVal, curVal, idx, obj) {
  result = (prevVal === 1 && obj[idx] === curVal);
}

[11].reduce(callbackfn, 1);

assert(result, 'result !== true');
