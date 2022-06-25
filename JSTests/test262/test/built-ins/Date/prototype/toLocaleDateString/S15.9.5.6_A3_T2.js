// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleDateString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.toLocaleDateString.length,
  true,
  'The value of `delete Date.prototype.toLocaleDateString.length` is expected to be true'
);

assert(
  !Date.prototype.toLocaleDateString.hasOwnProperty('length'),
  'The value of !Date.prototype.toLocaleDateString.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
