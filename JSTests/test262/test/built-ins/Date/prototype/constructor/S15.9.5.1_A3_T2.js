// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.constructor property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.constructor.length,
  true,
  'The value of `delete Date.prototype.constructor.length` is expected to be true'
);

assert(
  !Date.prototype.constructor.hasOwnProperty('length'),
  'The value of !Date.prototype.constructor.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
