// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setFullYear.length,
  true,
  'The value of `delete Date.prototype.setFullYear.length` is expected to be true'
);

assert(
  !Date.prototype.setFullYear.hasOwnProperty('length'),
  'The value of !Date.prototype.setFullYear.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
