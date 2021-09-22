// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object.prototype.hasOwnProperty can't be used as a constructor
es5id: 15.2.4.5_A7
description: Checking if creating "new Object.prototype.hasOwnProperty" fails
---*/

assert.throws(TypeError, function() {
  new Object.prototype.hasOwnProperty();
}, '`new Object.prototype.hasOwnProperty()` throws a TypeError exception');
