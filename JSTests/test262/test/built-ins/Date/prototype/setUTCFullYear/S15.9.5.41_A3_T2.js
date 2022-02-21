// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking DontDelete attribute
---*/
assert.sameValue(
  delete Date.prototype.setUTCFullYear.length,
  true,
  'The value of `delete Date.prototype.setUTCFullYear.length` is expected to be true'
);

assert(
  !Date.prototype.setUTCFullYear.hasOwnProperty('length'),
  'The value of !Date.prototype.setUTCFullYear.hasOwnProperty(\'length\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
