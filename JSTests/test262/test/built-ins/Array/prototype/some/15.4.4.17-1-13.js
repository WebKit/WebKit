// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: Array.prototype.some applied to the JSON object
---*/

function callbackfn(val, idx, obj) {
  return '[object JSON]' === Object.prototype.toString.call(obj);
}

JSON.length = 1;
JSON[0] = 1;

assert(Array.prototype.some.call(JSON, callbackfn), 'Array.prototype.some.call(JSON, callbackfn) !== true');
