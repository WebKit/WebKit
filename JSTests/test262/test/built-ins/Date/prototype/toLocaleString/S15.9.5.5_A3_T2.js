// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking DontDelete attribute
---*/

assert.sameValue(
  delete Date.prototype.toLocaleString.length,
  true,
  'The value of `delete Date.prototype.toLocaleString.length` is expected to be true'
);

assert(
  !Date.prototype.toLocaleString.hasOwnProperty('length'),
  'The value of !Date.prototype.toLocaleString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
