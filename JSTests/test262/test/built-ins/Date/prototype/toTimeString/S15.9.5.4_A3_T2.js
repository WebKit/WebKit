// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toTimeString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.totimestring
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.toTimeString.length,
  true,
  'The value of `delete Date.prototype.toTimeString.length` is expected to be true'
);

assert(
  !Date.prototype.toTimeString.hasOwnProperty('length'),
  'The value of !Date.prototype.toTimeString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
