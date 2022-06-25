// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: Array.prototype.some applied to RegExp object
---*/

function callbackfn(val, idx, obj) {
  return obj instanceof RegExp;
}

var obj = new RegExp();
obj.length = 2;
obj[0] = 11;
obj[1] = 9;

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
