// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The slice property of Array can't be used as constructor
esid: sec-array.prototype.slice
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

assert.throws(TypeError, () => {
  new Array.prototype.slice();
});
