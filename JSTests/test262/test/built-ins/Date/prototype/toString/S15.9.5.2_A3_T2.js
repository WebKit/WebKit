// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking DontDelete attribute
---*/

assert.sameValue(
  delete Date.prototype.toString.length,
  true,
  'The value of `delete Date.prototype.toString.length` is expected to be true'
);

assert(
  !Date.prototype.toString.hasOwnProperty('length'),
  'The value of !Date.prototype.toString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
