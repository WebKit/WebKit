// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toUTCString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking DontDelete attribute
---*/

assert.sameValue(
  delete Date.prototype.toUTCString.length,
  true,
  'The value of `delete Date.prototype.toUTCString.length` is expected to be true'
);

assert(
  !Date.prototype.toUTCString.hasOwnProperty('length'),
  'The value of !Date.prototype.toUTCString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
