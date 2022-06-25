// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: Array.prototype.some applied to boolean primitive
---*/

function callbackfn(val, idx, obj) {
  return obj instanceof Boolean;
}

Boolean.prototype[0] = 1;
Boolean.prototype.length = 1;

assert(Array.prototype.some.call(false, callbackfn), 'Array.prototype.some.call(false, callbackfn) !== true');
