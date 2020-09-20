// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The parseInt property can't be used as constructor
esid: sec-parseint-string-radix
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

assert.throws(TypeError, () => {
  new parseInt();
}, '`new parseInt()` throws TypeError');
