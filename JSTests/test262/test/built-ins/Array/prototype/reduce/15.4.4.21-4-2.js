// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reduce
description: >
    Array.prototype.reduce throws ReferenceError if callbackfn is
    unreferenced
---*/

var arr = new Array(10);
assert.throws(ReferenceError, function() {
  arr.reduce(foo);
});
