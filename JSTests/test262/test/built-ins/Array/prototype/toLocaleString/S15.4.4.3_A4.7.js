// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.tolocalestring
info: The toLocaleString property of Array can't be used as constructor
es5id: 15.4.4.3_A4.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

assert.throws(TypeError, () => {
  new Array.prototype.toLocaleString();
});
